#include <winstring.h>
#include "winnls.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(bcp47langs);

HRESULT WINAPI WINAPI GetUserLanguages(WCHAR delimiter, HSTRING *user_languages)
{
    HRESULT hr;
    WCHAR locale[LOCALE_NAME_MAX_LENGTH];

    if (!user_languages)
        return E_INVALIDARG;
    
    if (!GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH))
        return E_FAIL;

    if (FAILED(hr = WindowsCreateString(locale, wcslen(locale), user_languages)))
        return hr;

    return S_OK;
}
