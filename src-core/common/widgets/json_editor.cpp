#include "json_editor.h"
#include "core/style.h"
#include "core/ui_safety.h"
#include "i18n.h"
#include "imgui/imgui.h"
#include "imgui/imgui_stdlib.h"

namespace satdump
{
    namespace widgets
    {
        inline float InputWidthWithDeleteButton()
        {
            const ImGuiStyle &imgui_style = ImGui::GetStyle();
            const float delete_width = ImGui::CalcTextSize(u8"\uf00d").x + imgui_style.FramePadding.x * 2.0f + imgui_style.ItemSpacing.x;
            return ui_safety::fittedInputWidth(ImGui::GetContentRegionAvail().x, delete_width);
        }

        template <typename T>
        inline void AddButton(T &json, bool allow_add)
        {
            if (allow_add && ImGui::Button(_("Add...")))
                ImGui::OpenPopup(_("Add Item"));

            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal(_("Add Item"), NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                static std::string new_key = "new_item";
                if (json.is_object())
                {
                    ImGui::TextUnformatted(_("Key:"));
                    ImGui::SameLine();
                    ImGui::InputText("##label", &new_key, ImGuiInputTextFlags_AutoSelectAll);
                }

                static int selected = 0;
                ImGui::SameLine();
                ImGui::TextUnformatted(_("Type:"));
                ImGui::SameLine();
                const std::string type_options = std::string(_("Object")) + '\0' + _("Array") + '\0' + _("String") + '\0' + _("Integer") + '\0' + _("Double") + '\0' + _("Boolean") + '\0';
                ImGui::Combo("##Type", &selected, type_options.c_str());
                ImGuiStyle &style = ImGui::GetStyle();
                ImGui::SetCursorPos({(ImGui::GetContentRegionAvail().x / 2) - ((ImGui::CalcTextSize(_("Add")).x + ImGui::CalcTextSize(_("Cancel")).x + style.ItemSpacing.x + style.FramePadding.x * 4) / 2),
                                     ImGui::GetCursorPosY() + 5.0f * ui_scale});

                if (ImGui::Button(_("Add")))
                {
                    if (json.is_object())
                    {
                        switch (selected)
                        {
                        case 0:
                            json[new_key] = T::object();
                            break;
                        case 1:
                            json[new_key] = T::array();
                            break;
                        case 2:
                            json[new_key] = "";
                            break;
                        case 3:
                            json[new_key] = 0;
                            break;
                        case 4:
                            json[new_key] = 0.0;
                            break;
                        case 5:
                            json[new_key] = false;
                        }
                    }
                    else
                    {
                        switch (selected)
                        {
                        case 0:
                            json.push_back(T::object());
                            break;
                        case 1:
                            json.push_back(T::array());
                            break;
                        case 2:
                            json.push_back("");
                            break;
                        case 3:
                            json.push_back(0);
                            break;
                        case 4:
                            json.push_back(0.0);
                            break;
                        case 5:
                            json.push_back(false);
                        }
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button(_("Cancel")))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        [[nodiscard]] inline bool DeleteButton()
        {
            ImGui::SameLine();
            ImGui::SetCursorPosY(ImGui::GetCursorPos().y - 2 * ui_scale);
            ImGui::PushStyleColor(ImGuiCol_Text, style::theme.red.Value);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4());
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
            bool ret = ImGui::Button(u8"\uf00d");
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            return ret;
        }

        template <typename T>
        void JSONTreeEditor(T &json, const char *id, bool allow_add)
        {
            ImGui::PushID(id);
            int array_index = 0;
            float start_pos = ImGui::GetCursorPosX();
            bool delete_item = false;

            for (auto jsonItem = json.begin(); jsonItem != json.end();)
            {
                std::string this_key;
                if (json.is_array())
                    this_key = std::to_string(array_index++);
                else
                    this_key = jsonItem.key();

                ImGui::PushID(this_key.c_str());
                if (jsonItem.value().is_object())
                {
                    bool nodeOpen = ImGui::TreeNode(std::string((json.is_array() ? "##" : "") + this_key).c_str());
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s", _("[Object]"));
                    delete_item = DeleteButton();
                    if (nodeOpen)
                    {
                        JSONTreeEditor(jsonItem.value(), this_key.c_str());
                        ImGui::TreePop();
                    }
                }
                else if (jsonItem.value().is_array())
                {
                    bool nodeOpen = ImGui::TreeNode(std::string((json.is_array() ? "##" : "") + this_key).c_str());
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s", jsonItem.value().dump().c_str());
                    delete_item = DeleteButton();
                    if (nodeOpen)
                    {
                        JSONTreeEditor(jsonItem.value(), this_key.c_str());
                        ImGui::TreePop();
                    }
                }
                else if (jsonItem.value().is_boolean())
                {
                    if (!json.is_array())
                    {
                        ImGui::BulletText("%s", this_key.c_str());
                        ImGui::SameLine();
                    }
                    bool val = jsonItem.value();
                    if (ImGui::Checkbox(std::string("##" + this_key).c_str(), &val))
                        jsonItem.value() = val;
                    delete_item = DeleteButton();
                }
                else if (jsonItem.value().is_number_integer() || jsonItem.value().is_number_unsigned())
                {
                    if (!json.is_array())
                    {
                        ImGui::BulletText("%s", this_key.c_str());
                        ImGui::SameLine();
                    }

                    ImGui::SetNextItemWidth(InputWidthWithDeleteButton());
                    if (jsonItem.value().is_number_unsigned())
                    {
                        uint64_t val = jsonItem.value().template get<uint64_t>();
                        if (ImGui::InputScalar(std::string("##" + this_key).c_str(), ImGuiDataType_U64, &val))
                            jsonItem.value() = val;
                    }
                    else
                    {
                        int64_t val = jsonItem.value().template get<int64_t>();
                        if (ImGui::InputScalar(std::string("##" + this_key).c_str(), ImGuiDataType_S64, &val))
                            jsonItem.value() = val;
                    }
                    delete_item = DeleteButton();
                }
                else if (jsonItem.value().is_number_float())
                {
                    if (!json.is_array())
                    {
                        ImGui::BulletText("%s", this_key.c_str());
                        ImGui::SameLine();
                    }

                    ImGui::SetNextItemWidth(InputWidthWithDeleteButton());
                    double val = jsonItem.value();
                    if (ImGui::InputDouble(std::string("##" + this_key).c_str(), &val))
                        jsonItem.value() = val;
                    delete_item = DeleteButton();
                }
                else if (jsonItem.value().is_string())
                {
                    if (!json.is_array())
                    {
                        ImGui::BulletText("%s", this_key.c_str());
                        ImGui::SameLine();
                    }

                    ImGui::SetNextItemWidth(InputWidthWithDeleteButton());
                    std::string val = jsonItem.value();
                    if (val.find("\n") == std::string::npos ? ImGui::InputText(std::string("##" + this_key).c_str(), &val) : ImGui::InputTextMultiline(std::string("##" + this_key).c_str(), &val))
                        jsonItem.value() = val;
                    delete_item = DeleteButton();
                }

                ImGui::PopID();
                if (delete_item)
                {
                    jsonItem = json.erase(jsonItem);
                    delete_item = false;
                }
                else
                    jsonItem++;
            }

            AddButton<T>(json, allow_add);
            ImGui::PopID();
        }

        bool JSONTableEditor(nlohmann::json &json, const char *id)
        {
            ImGui::PushID(id);
            ImVec2 start_pos = ImGui::GetCursorPos();
            ImGui::SetCursorPos({start_pos.x + 3 * ui_scale, start_pos.y + 10 * ui_scale});
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(id);
            ImGui::SameLine();
            AddButton<nlohmann::json>(json, true);
            ImGui::SameLine();
            bool ret = ImGui::Button(std::string(std::string(_("Reset")) + "##" + id).c_str());

            if (json.size() > 0)
            {
                const float table_width = ImGui::GetContentRegionAvail().x;
                if (ImGui::BeginTable(std::string(std::string(id) + "##table").c_str(), 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("##json_key", ImGuiTableColumnFlags_WidthFixed,
                                            ui_safety::labelColumnWidth(table_width, ui_scale));
                    ImGui::TableSetupColumn("##json_value", ImGuiTableColumnFlags_WidthStretch);
                    int array_index = 0;
                    bool delete_item = false;

                    for (auto jsonItem = json.begin(); jsonItem != json.end();)
                    {
                        std::string this_key;
                        if (json.is_array())
                            this_key = std::to_string(array_index++);
                        else
                            this_key = jsonItem.key();

                        ImGui::PushID(this_key.c_str());
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        if (!json.is_array())
                            ImGui::TextUnformatted(this_key.c_str());

                        ImGui::TableSetColumnIndex(1);
                        if (jsonItem.value().is_object())
                        {
                            ImGui::TextDisabled("%s", _("[Object]"));
                            delete_item = DeleteButton();
                            JSONTreeEditor(jsonItem.value(), this_key.c_str());
                        }
                        else if (jsonItem.value().is_array())
                        {
                            ImGui::TextDisabled("%s", jsonItem.value().dump().c_str());
                            delete_item = DeleteButton();
                            JSONTreeEditor(jsonItem.value(), this_key.c_str());
                        }
                        else if (jsonItem.value().is_boolean())
                        {
                            bool val = jsonItem.value();
                            if (ImGui::Checkbox(std::string("##" + this_key).c_str(), &val))
                                jsonItem.value() = val;
                            delete_item = DeleteButton();
                        }
                        else if (jsonItem.value().is_number_integer() || jsonItem.value().is_number_unsigned())
                        {
                            ImGui::SetNextItemWidth(InputWidthWithDeleteButton());
                            if (jsonItem.value().is_number_unsigned())
                            {
                                uint64_t val = jsonItem.value().get<uint64_t>();
                                if (ImGui::InputScalar(std::string("##" + this_key).c_str(), ImGuiDataType_U64, &val))
                                    jsonItem.value() = val;
                            }
                            else
                            {
                                int64_t val = jsonItem.value().get<int64_t>();
                                if (ImGui::InputScalar(std::string("##" + this_key).c_str(), ImGuiDataType_S64, &val))
                                    jsonItem.value() = val;
                            }
                            delete_item = DeleteButton();
                        }
                        else if (jsonItem.value().is_number_float())
                        {
                            double val = jsonItem.value();
                            ImGui::SetNextItemWidth(InputWidthWithDeleteButton());
                            if (ImGui::InputDouble(std::string("##" + this_key).c_str(), &val))
                                jsonItem.value() = val;
                            delete_item = DeleteButton();
                        }
                        else if (jsonItem.value().is_string())
                        {
                            std::string val = jsonItem.value();
                            ImGui::SetNextItemWidth(InputWidthWithDeleteButton());
                            if (val.find("\n") == std::string::npos ? ImGui::InputText(std::string("##" + this_key).c_str(), &val)
                                                                    : ImGui::InputTextMultiline(std::string("##" + this_key).c_str(), &val))
                                jsonItem.value() = val;
                            delete_item = DeleteButton();
                        }

                        ImGui::PopID();
                        if (delete_item)
                        {
                            jsonItem = json.erase(jsonItem);
                            delete_item = false;
                        }
                        else
                            jsonItem++;
                    }

                    ImGui::EndTable();
                }
            }
            else
                ImGui::Separator();

            ImGui::PopID();
            return ret;
        }

        template void JSONTreeEditor(nlohmann::ordered_json &, const char *, bool);
        template void JSONTreeEditor(nlohmann::json &, const char *, bool);
    } // namespace widgets
} // namespace satdump
