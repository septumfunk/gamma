# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/djf/Code/alpha/build/_deps/sf-std-src")
  file(MAKE_DIRECTORY "/Users/djf/Code/alpha/build/_deps/sf-std-src")
endif()
file(MAKE_DIRECTORY
  "/Users/djf/Code/alpha/build/_deps/sf-std-build"
  "/Users/djf/Code/alpha/build/_deps/sf-std-subbuild/sf-std-populate-prefix"
  "/Users/djf/Code/alpha/build/_deps/sf-std-subbuild/sf-std-populate-prefix/tmp"
  "/Users/djf/Code/alpha/build/_deps/sf-std-subbuild/sf-std-populate-prefix/src/sf-std-populate-stamp"
  "/Users/djf/Code/alpha/build/_deps/sf-std-subbuild/sf-std-populate-prefix/src"
  "/Users/djf/Code/alpha/build/_deps/sf-std-subbuild/sf-std-populate-prefix/src/sf-std-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/djf/Code/alpha/build/_deps/sf-std-subbuild/sf-std-populate-prefix/src/sf-std-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/djf/Code/alpha/build/_deps/sf-std-subbuild/sf-std-populate-prefix/src/sf-std-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
