add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

option("target_type")
    set_default("server")
    set_showmenu(true)
    set_values("server", "client")
option_end()

add_requires("levilamina", {configs = {target_type = get_config("target_type")}})

package("protocol")
    set_description("SculkCatalystMC Minecraft Bedrock Protocol Library")
    set_license("MPL-2.0")
    set_homepage("https://github.com/SculkCatalystMC/Protocol")
    add_urls("https://github.com/SculkCatalystMC/Protocol.git")

    add_configs("enable_codec",           {description = "Enable packet serialization/codec",              default = true,  type = "boolean"})
    add_configs("enable_authentication",  {description = "Enable Xbox authentication",                     default = false, type = "boolean"})
    add_configs("enable_connection",      {description = "Enable RakNet-based networking",                 default = false, type = "boolean"})
    add_configs("enable_detail_errors",   {description = "Enable detailed errors with source_location",    default = false, type = "boolean"})
    add_configs("enable_exceptions",      {description = "Enable C++ exceptions",                          default = false, type = "boolean"})
    add_configs("enable_formatting",      {description = "Enable fmt/std::format support (requires boost)", default = false, type = "boolean"})

    set_sourcedir(path.join(os.scriptdir(), "..", "..", "SculkCatalystMC", "Protocol"))

    on_load(function (package)
        if package:config("enable_connection") and not package:config("enable_codec") then
            raise("enable_connection requires enable_codec")
        end
        if package:config("enable_formatting") then
            package:add("deps", "boost_pfr")
        end
        if package:config("enable_authentication") or package:config("enable_connection") then
            package:add("deps", "openssl3 >=3.0.0")
        end
    end)

    on_install(function (package)
        local configs = {
            "-DPROTOCOL_ENABLE_CODEC="          .. (package:config("enable_codec")          and "ON" or "OFF"),
            "-DPROTOCOL_ENABLE_AUTHENTICATION=" .. (package:config("enable_authentication") and "ON" or "OFF"),
            "-DPROTOCOL_ENABLE_CONNECTION="     .. (package:config("enable_connection")     and "ON" or "OFF"),
            "-DPROTOCOL_ENABLE_DETAIL_ERRORS="  .. (package:config("enable_detail_errors")  and "ON" or "OFF"),
            "-DPROTOCOL_ENABLE_EXCEPTIONS="     .. (package:config("enable_exceptions")     and "ON" or "OFF"),
            "-DPROTOCOL_ENABLE_FORMATTING="     .. (package:config("enable_formatting")     and "ON" or "OFF"),
        }
        if package:config("enable_formatting") then
            local boost_pfr = package:dep("boost_pfr")
            if boost_pfr then
                table.insert(configs, "-DCMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES=" .. path.join(boost_pfr:installdir(), "include"))
            end
        end
        import("package.tools.cmake").install(package, configs)
        os.cp(path.join(package:sourcedir(), "include", "sculk", "protocol", "*.hpp"), path.join(package:installdir("include"), "sculk", "protocol"))
        os.cp(path.join(package:sourcedir(), "include", "sculk", "protocol", "utility"), path.join(package:installdir("include"), "sculk", "protocol"))
    end)

    on_test(function (package)
        package:check_cxxsnippets({test = [[
            #include <sculk/protocol/Version.hpp>
            #include <sculk/protocol/codec/MinecraftPacketIds.hpp>
            void test() {
                static_assert(sculk::protocol::MinecraftPacketIds::Login == 1);
                static_assert(sculk::protocol::getProtocolVersion() == 975);
            }
        ]]}, {configs = {languages = "c++23"}})
    end)
package_end()

add_requires("protocol b64ddd67f7a59dd25b17e762698d7017b8b04d84", {
    configs = {
        enable_detail_errors = true,
        enable_codec = true,
        enable_authentication = false,
        enable_connection = false,
        enable_exceptions = false,
        enable_formatting = true
    }
})

add_requires("levibuildscript")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("Phantom")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    add_cxflags("/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
    add_defines("NOMINMAX", "UNICODE")
    add_packages("levilamina", "protocol")
    set_exceptions("none")
    set_kind("shared")
    set_languages("c++23")
    set_symbols("debug")
    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    add_includedirs("src")
