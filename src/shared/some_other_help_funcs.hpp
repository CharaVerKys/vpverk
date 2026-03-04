#pragma once

#include <filesystem>

#ifdef _WIN32
extern HINSTANCE hInstance; // i have no idea what is this
namespace cvk{
    namespace details::external{
        // i not read this function

        // Source - https://stackoverflow.com/a/54491532
        template <typename TChar, typename TStringGetterFunc>
        std::basic_string<TChar> GetStringFromWindowsApi( TStringGetterFunc stringGetter, int initialSize = 0 )
        {
            if( initialSize <= 0 )
            {
                initialSize = MAX_PATH;
            }

            std::basic_string<TChar> result( initialSize, 0 );
            for(;;)
            {
                auto length = stringGetter( &result[0], result.length() );
                if( length == 0 )
                {
                    return std::basic_string<TChar>();
                }

                if( length < result.length() - 1 )
                {
                    result.resize( length );
                    result.shrink_to_fit();
                    return result;
                }

                result.resize( result.length() * 2 );
            }
        }
    }//nms details::external
#else
namespace cvk{
#endif //_WIN32




    inline std::filesystem::path get_current_exec_path(){
#ifdef _WIN32
        // Source - https://stackoverflow.com/a/54491532
        // i literally have no idea what happening here, not want to read this, fck windows
        auto moduleName = details::external::GetStringFromWindowsApi<TCHAR>( []( TCHAR* buffer, int size )
        {
            return GetModuleFileName( hInstance, buffer, size );
        } );
        return std::filesystem::path(moduleName);
#elif defined __linux__
    return std::filesystem::read_symlink("/proc/self/exe");
#else
    static_assert(false,"unsupported platform");
#endif
    }// end get_current_exec_path


}// nms cvk
