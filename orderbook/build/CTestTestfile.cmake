# CMake generated Testfile for 
# Source directory: /Users/khuongle/Documents/orderbook
# Build directory: /Users/khuongle/Documents/orderbook/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(m1_reference "/Users/khuongle/Documents/orderbook/build/test_engine_ref")
set_tests_properties(m1_reference PROPERTIES  _BACKTRACE_TRIPLES "/Users/khuongle/Documents/orderbook/CMakeLists.txt;15;add_test;/Users/khuongle/Documents/orderbook/CMakeLists.txt;0;")
add_test(m2_reference "/Users/khuongle/Documents/orderbook/build/test_engine_m2")
set_tests_properties(m2_reference PROPERTIES  _BACKTRACE_TRIPLES "/Users/khuongle/Documents/orderbook/CMakeLists.txt;23;add_test;/Users/khuongle/Documents/orderbook/CMakeLists.txt;0;")
add_test(m3_spsc "/Users/khuongle/Documents/orderbook/build/test_spsc")
set_tests_properties(m3_spsc PROPERTIES  _BACKTRACE_TRIPLES "/Users/khuongle/Documents/orderbook/CMakeLists.txt;29;add_test;/Users/khuongle/Documents/orderbook/CMakeLists.txt;0;")
