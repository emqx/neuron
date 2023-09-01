# Test
Unit testing uses googletest testing framework, testing point processing, JSON reading, architecture, HTTP acquisition, and other small-scale tests.
To run all unit tests.

```shell
$ cd build
$ ctest --output-on-failure
```

To introduce coverage statistics using LCOV:

Add parameter -DUSE_GCOV=1 during compilation

Generate image files .gcno and data files .gcda under build/tests/CMakeFiles/.dir

Execute the following command in the build folder to generate an intermediate file for HTML:

```shell
$ lcov --capture --directory . --output-file testneuron.info --test-name testneuron
```

Execute the following command in the build folder to generate an HTML file in the result folder under the build folder:

```shell
$ genhtml -o result testneuron.info
```

Open build/result index.html to view coverage statistics report.