# enable packing
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VENDOR "RigelEngine: Proton contributors")
set(CPACK_PACKAGE_CONTACT "shimian5@users.noreply.github.com")
set(CPACK_PACKAGE_NAME "rigelengine-proton")
set(CPACK_PACKAGE_DESCRIPTION [[
RigelEngine: Proton is an enhanced engine for Duke Nukem I and II, based on
RigelEngine. It offers a shared launcher, modern menus and HUD, controller
support, widescreen rendering, motion smoothing and persistent saves.

Players supply their own original shareware or registered DOS game data.
No game files are included.
]])
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Enhanced engine for Duke Nukem I and II")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/shimian5/RigelEngine-Proton")
set(CPACK_PACKAGE_CHECKSUM "SHA256")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "RigelEngine-Proton")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_EXECUTABLES "rigelengine-proton" "RigelEngine: Proton")
set(CPACK_CREATE_DESKTOP_LINKS "rigelengine-proton")
set(CPACK_OUTPUT_FILE_PREFIX packages)
set(CPACK_STRIP_FILES TRUE)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL bin)
set(CPACK_PACKAGE_FILE_NAME "rigelengine-proton")

# We don't include this in order to avoid popping up a "Agree to EULA" dialog
# for some CPack generators, like Mac OS
#set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.md")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")

# a headsup
set(CPACK_WARN_ON_ABSOLUTE_INSTALL_DESTINATION TRUE)

# Debian
# (abencsik) intentionally left this here for future reference
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.27), libstdc++6 (>= 8.4.0), libsdl2-2.0-0 (>= 2.0.4), libsdl2-mixer-2.0-0 (>= 2.0.1)")
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "")

include(CPack)
