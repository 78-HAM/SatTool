#include "libs/libintl-tiny/libintl.h"

#include <cassert>
#include <cstdlib>
#include <string>

int main()
{
    const char *catalog_root = "resources/i18n";

#ifdef _WIN32
    _putenv_s("LANGUAGE", "zh_CN");
#else
    setenv("LANGUAGE", "zh_CN", 1);
#endif
    assert(bindtextdomain("satdump", catalog_root));
    textdomain("satdump");
    assert(std::string(gettext("Settings")) == "\xE8\xAE\xBE\xE7\xBD\xAE");

#ifdef _WIN32
    _putenv_s("LANGUAGE", "en");
#else
    setenv("LANGUAGE", "en", 1);
#endif
    closeLoadedMessageCatalog("satdump");
    assert(!bindtextdomain("satdump", catalog_root));
    textdomain("satdump");
    assert(std::string(gettext("Settings")) == "Settings");
}
