# aos-synchronization
C++ implementation of popular process synchronization algorithms as part of Advanced Operating Systems course at Faculty of Electrical Engineering and Computing, University of Zagreb, academic year 2024./2025.

<br />

The programs take no arguments and number of philosophers is set to 5. Since all created processes have the same starting timestamp, the program always runs in the same order even when in theory, two non-adjacent philosophers could eat at the same time.

<br />

Message queue variant implements Lamport's distributed algorithm while the pipeline variant showcases implementation of Ricart-Agrawala.

<br />

To run the program on **Windows** you need to use **Windows Subsystem for Linux** or **WSL** (since some of the libraries used are not a standard package of C++ compiler on Windows) and execute:

`g++ -o <output> <input>.cpp -lrt`
