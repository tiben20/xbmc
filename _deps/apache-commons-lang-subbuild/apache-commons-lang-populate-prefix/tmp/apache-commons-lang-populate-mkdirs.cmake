# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "H:/kodidsplayermaster/project/BuildDependencies/x64/share/java/lang"
  "H:/kodidsplayermaster/_deps/apache-commons-lang-build"
  "H:/kodidsplayermaster/_deps/apache-commons-lang-subbuild/apache-commons-lang-populate-prefix"
  "H:/kodidsplayermaster/_deps/apache-commons-lang-subbuild/apache-commons-lang-populate-prefix/tmp"
  "H:/kodidsplayermaster/_deps/apache-commons-lang-subbuild/apache-commons-lang-populate-prefix/src/apache-commons-lang-populate-stamp"
  "H:/kodidsplayermaster/project/BuildDependencies/downloads"
  "H:/kodidsplayermaster/_deps/apache-commons-lang-subbuild/apache-commons-lang-populate-prefix/src/apache-commons-lang-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "H:/kodidsplayermaster/_deps/apache-commons-lang-subbuild/apache-commons-lang-populate-prefix/src/apache-commons-lang-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "H:/kodidsplayermaster/_deps/apache-commons-lang-subbuild/apache-commons-lang-populate-prefix/src/apache-commons-lang-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
