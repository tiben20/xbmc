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

project(apache-commons-lang-populate NONE)



include(ExternalProject)
ExternalProject_Add(apache-commons-lang-populate
                     "UPDATE_DISCONNECTED" "False" "DOWNLOAD_EXTRACT_TIMESTAMP" "FALSE" "EXTERNALPROJECT_INTERNAL_ARGUMENT_SEPARATOR" "URL" "http://mirrors.kodi.tv/build-deps/sources/commons-lang3-3.14.0-bin.tar.gz" "URL_HASH" "SHA512=8e7e62418a49ba810512c13a640a8bf35f878fcd54af32fdaab1111e37817f58b21b475980ba663fba4887e45ef8d88af8ff17796f20d202e929e8e2574546dc" "DOWNLOAD_DIR" "H:/kodidsplayermaster/project/BuildDependencies/downloads"
                    SOURCE_DIR          "H:/kodidsplayermaster/project/BuildDependencies/x64/share/java/lang"
                    BINARY_DIR          "H:/kodidsplayermaster/_deps/apache-commons-lang-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
                    USES_TERMINAL_PATCH     YES
)


