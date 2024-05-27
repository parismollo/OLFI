# Benchmark 
## Read and Write 1.3
```
root@pnl v4]# ./test
------------START BENCHMARK V4------------
(1) create_file_with_random_writes():
create_file_with_random_writes() - Elapsed time: 0.009684 seconds
(2) create_file_with_sequential_writes():
create_file_with_sequential_writes() - Elapsed time: 0.000693 seconds
(3) read_file_and_measure_performance():
random - Elapsed time: 0.000736 seconds
(4) read_file_and_measure_performance():
sequential - Elapsed time: 0.000109 seconds
(5) test_fragmented_write():
test_fragmented_write() - Elapsed time: 0.001113 seconds
(9) create_multiple_files_with_sequential_writes():
create_file_with_sequential_writes() - Elapsed time: 0.000585 seconds
create_file_with_sequential_writes() - Elapsed time: 0.000622 seconds
create_file_with_sequential_writes() - Elapsed time: 0.000556 seconds
create_file_with_sequential_writes() - Elapsed time: 0.000599 seconds
create_file_with_sequential_writes() - Elapsed time: 0.000385 seconds
Average elapsed time for 5 files: 0.001579 seconds
(10) create_multiple_files_with_random_writes():
create_file_with_random_writes() - Elapsed time: 0.008845 seconds
create_file_with_random_writes() - Elapsed time: 0.008375 seconds
create_file_with_random_writes() - Elapsed time: 0.008480 seconds
create_file_with_random_writes() - Elapsed time: 0.008705 seconds
create_file_with_random_writes() - Elapsed time: 0.007954 seconds
Average elapsed time for 5 files: 0.009298 seconds
(11) create_multiple_files_with_fragmented_writes():
test_fragmented_write() - Elapsed time: 0.000516 seconds
test_fragmented_write() - Elapsed time: 0.000458 seconds
test_fragmented_write() - Elapsed time: 0.000593 seconds
test_fragmented_write() - Elapsed time: 0.000462 seconds
test_fragmented_write() - Elapsed time: 0.000446 seconds
Average elapsed time for 5 files: 0.001931 seconds
(12) read_multiple_files_and_measure_performance():
File read - Elapsed time: 0.000152 seconds
File read - Elapsed time: 0.000103 seconds
File read - Elapsed time: 0.000102 seconds
File read - Elapsed time: 0.000152 seconds
File read - Elapsed time: 0.000306 seconds
Average elapsed time for 5 files: 0.000702 seconds
(13) read_multiple_files_and_measure_performance():
File read - Elapsed time: 0.000115 seconds
File read - Elapsed time: 0.000102 seconds
File read - Elapsed time: 0.000123 seconds
File read - Elapsed time: 0.000151 seconds
File read - Elapsed time: 0.000071 seconds
Average elapsed time for 5 files: 0.000436 seconds
(14) read_multiple_files_and_measure_performance():
File read - Elapsed time: 0.000074 seconds
File read - Elapsed time: 0.000072 seconds
File read - Elapsed time: 0.000073 seconds
File read - Elapsed time: 0.000096 seconds
File read - Elapsed time: 0.000126 seconds
Average elapsed time for 5 files: 0.000455 seconds
(15) verify_write_and_read():
verify_write_and_read(): OK!
------------END BENCHMARK V4------------
```

## Default Linux
```
------------START BENCHMARK V4------------
(1) create_file_with_random_writes():
create_file_with_random_writes() - Elapsed time: 0.002687 seconds
(2) create_file_with_sequential_writes():
create_file_with_sequential_writes() - Elapsed time: 0.000233 seconds
(3) read_file_and_measure_performance():
random - Elapsed time: 0.000813 seconds
(4) read_file_and_measure_performance():
sequential - Elapsed time: 0.000083 seconds
(5) test_fragmented_write():
test_fragmented_write() - Elapsed time: 0.000725 seconds
(9) create_multiple_files_with_sequential_writes():
create_file_with_sequential_writes() - Elapsed time: 0.000159 seconds
create_file_with_sequential_writes() - Elapsed time: 0.000142 seconds
create_file_with_sequential_writes() - Elapsed time: 0.000142 seconds
create_file_with_sequential_writes() - Elapsed time: 0.000112 seconds
create_file_with_sequential_writes() - Elapsed time: 0.000110 seconds
Average elapsed time for 5 files: 0.000869 seconds
(10) create_multiple_files_with_random_writes():
create_file_with_random_writes() - Elapsed time: 0.000984 seconds
create_file_with_random_writes() - Elapsed time: 0.001095 seconds
create_file_with_random_writes() - Elapsed time: 0.000953 seconds
create_file_with_random_writes() - Elapsed time: 0.001548 seconds
create_file_with_random_writes() - Elapsed time: 0.000999 seconds
Average elapsed time for 5 files: 0.001948 seconds
(11) create_multiple_files_with_fragmented_writes():
test_fragmented_write() - Elapsed time: 0.000329 seconds
test_fragmented_write() - Elapsed time: 0.000194 seconds
test_fragmented_write() - Elapsed time: 0.000186 seconds
test_fragmented_write() - Elapsed time: 0.000299 seconds
test_fragmented_write() - Elapsed time: 0.000242 seconds
Average elapsed time for 5 files: 0.001270 seconds
(12) read_multiple_files_and_measure_performance():
File read - Elapsed time: 0.000128 seconds
File read - Elapsed time: 0.000079 seconds
File read - Elapsed time: 0.000081 seconds
File read - Elapsed time: 0.000095 seconds
File read - Elapsed time: 0.000089 seconds
Average elapsed time for 5 files: 0.000533 seconds
(13) read_multiple_files_and_measure_performance():
File read - Elapsed time: 0.000094 seconds
File read - Elapsed time: 0.000094 seconds
File read - Elapsed time: 0.000093 seconds
File read - Elapsed time: 0.000094 seconds
File read - Elapsed time: 0.000092 seconds
Average elapsed time for 5 files: 0.000457 seconds
(14) read_multiple_files_and_measure_performance():
File read - Elapsed time: 0.000072 seconds
File read - Elapsed time: 0.000073 seconds
File read - Elapsed time: 0.000070 seconds
File read - Elapsed time: 0.000069 seconds
File read - Elapsed time: 0.000071 seconds
Average elapsed time for 5 files: 0.000344 seconds
(15) verify_write_and_read():
verify_write_and_read(): OK!
------------END BENCHMARK V4------------
```

## Fragmented