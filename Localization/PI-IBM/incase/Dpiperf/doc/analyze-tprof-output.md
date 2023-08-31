# Analyze PI Output

In order to analyze and visualize the performance inspector output, you can use [analyze-tprof-output.py] python tool.

## Description

The tool takes one or more PI output file(s) and analyzes the libraries and symbols of the Java process tracked in the file(s) (hence, throws an exception if no Java process was tracked in at least one of the output files). The tool produces the following 2 tables:

Library table:
| Library Name | Percentage Load |
| --- | --- |

Symbol Table:
| Library Name | Symbol Name | Percentage Load |
| --- | --- | --- |

* The first table is basically a summary of the second table (all symbols are aggregated).
* Library/Symbol Name: Name of the component being measured.
* Percentage Load: percentage of ticks spent on this symbol/process, relative to the total number of ticks spent by the Java process.
* The number of columns of the tables depends on the number of files/arguments entered when running the tool.
* In the Symbol Table, all libraries that have 0% load in all the input files are not recorded in the table. This is not the case, however, with the Library table (i.e.: libraries with all zeros will be printed in the Library tables, when the right configuration is set)

## Usage

`python2.7 analyze-tprof-output.py library_threshold symbols_threshold file_paths`

* Library Threshold: Minimum percentage value, above which libraries are recorded in the table. In other words, if a library has a load percentage >= library threshold in any of the provided output files, the library will appear in the table, otherwise it will be discarded.
* Symbol threshold: Minimum percentage value, above which symbols are recorded separately in the table. If a symbol has a percentage less than the symbol threshold, the symbol will be considered insignificant to be put in the table. All insignificant symbols have their values aggregated, according to which library they belong to.
* FilePaths: list of paths (minimum one, paths separated by spaces) of PI output files. Inputting more than one file path results in a table comparing all the files.