All the code to run the benchmark for ASCON is in this folder. aead.c and aead_updated.c is what you need to compile and execute on your computers and the rest of the files are the dependancies required to run both codes so you will need to include them when compiling.

aead.c : benchmarking using the driectory to files on your system. While this works it has its limitations when wanting to go bigger and bigger. and is not as robust.
aead_updated.c : this code allows for processing "large" amounts of data by simply iterating benchmarking 1MB of data on a loop until you process the amount you desired

Whats more this also allows us to benchmark multiple cores by using threads. Where 1 thread = 1 core. But you need to perform encryption and decryption seperately which you will specifiy when running it.

This is the syntax you want to use when executing the program. <your_program> <Data in MB> <Number of Cores> <Mode>

<your_program> : depending on the platform you use to code, compile, and execute this will vary. (I use VS code so its aead.exe/aead_updated.exe)

<Data in MB> : Just the whole number of MB you want to process ie 100

<Number of Cores> : The number of threads the program will use to process the data also a whole number. Be sure to not exceed the number of cores just for consistency.

<Mode> : Whether you are benchmarking encryption or decryption where 0 = ecyrption and 1 = decryption.

You should get an output like this

aead_updated.exe 100 1 1
Running multithreaded decryption with 1 threads...
Multithreaded decryption time for 100 MB: 7.537000 seconds
Throughput: 13.267878 MB/s
