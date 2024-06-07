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

project(groovy-populate NONE)



include(ExternalProject)
ExternalProject_Add(groovy-populate
                     "UPDATE_DISCONNECTED" "False" "DOWNLOAD_EXTRACT_TIMESTAMP" "FALSE" "EXTERNALPROJECT_INTERNAL_ARGUMENT_SEPARATOR" "URL" "http://mirrors.kodi.tv/build-deps/sources/apache-groovy-binary-4.0.16.zip" "URL_HASH" "SHA512=743698979c801f3cfb36545888b5303e1e25dae5692bb45cab7ebbe102a6eda31f09abc6978349ad90ecaf630416fc87eacba7781244855e7e771a3c44041fa0" "DOWNLOAD_DIR" "H:/kodidsplayermaster/project/BuildDependencies/downloads"
                    SOURCE_DIR          "H:/kodidsplayermaster/project/BuildDependencies/x64/share/groovy"
                    BINARY_DIR          "H:/kodidsplayermaster/_deps/groovy-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
                    USES_TERMINAL_PATCH     YES
)


