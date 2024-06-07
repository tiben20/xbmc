# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.28.3)

# Reject any attempt to use a toolchain file. We must not use one because
# we could be downloading it here. If the CMAKE_TOOLCHAIN_FILE environment
# variable is set, the cache variable will have been initialized from it.
unset(CMAKE_TOOLCHAIN_FILE CACHE)
unset(ENV{CMAKE_TOOLCHAIN_FILE})

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(apache-commons-text-populate NONE)



include(ExternalProject)
ExternalProject_Add(apache-commons-text-populate
                     "UPDATE_DISCONNECTED" "False" "DOWNLOAD_EXTRACT_TIMESTAMP" "FALSE" "EXTERNALPROJECT_INTERNAL_ARGUMENT_SEPARATOR" "URL" "http://mirrors.kodi.tv/build-deps/sources/commons-text-1.11.0-bin.tar.gz" "URL_HASH" "SHA512=2e94877000dd270b69e2e8cbf49f258a90b4c628b6b6b0814e300a2f0e9c391f0816dceb0707e596ae3b7c9532f93e7a4917df47c77f44b3a810e14042ce5f3f" "DOWNLOAD_DIR" "H:/kodidsplayermaster/project/BuildDependencies/downloads"
                    SOURCE_DIR          "H:/kodidsplayermaster/project/BuildDependencies/x64/share/java/text"
                    BINARY_DIR          "H:/kodidsplayermaster/_deps/apache-commons-text-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
                    USES_TERMINAL_PATCH     YES
)


