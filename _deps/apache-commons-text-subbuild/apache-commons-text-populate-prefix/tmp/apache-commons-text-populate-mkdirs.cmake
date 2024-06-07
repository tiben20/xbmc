# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "H:/kodidsplayermaster/project/BuildDependencies/x64/share/java/text"
  "H:/kodidsplayermaster/_deps/apache-commons-text-build"
  "H:/kodidsplayermaster/_deps/apache-commons-text-subbuild/apache-commons-text-populate-prefix"
  "H:/kodidsplayermaster/_deps/apache-commons-text-subbuild/apache-commons-text-populate-prefix/tmp"
  "H:/kodidsplayermaster/_deps/apache-commons-text-subbuild/apache-commons-text-populate-prefix/src/apache-commons-text-populate-stamp"
  "H:/kodidsplayermaster/project/BuildDependencies/downloads"
  "H:/kodidsplayermaster/_deps/apache-commons-text-subbuild/apache-commons-text-populate-prefix/src/apache-commons-text-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "H:/kodidsplayermaster/_deps/apache-commons-text-subbuild/apache-commons-text-populate-prefix/src/apache-commons-text-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "H:/kodidsplayermaster/_deps/apache-commons-text-subbuild/apache-commons-text-populate-prefix/src/apache-commons-text-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
