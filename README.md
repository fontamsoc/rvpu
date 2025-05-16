RISC-V RTOS-enabled multi-core SoC with Wishbone4 peripherals.

CPU is a single-issue in-order 4 stages pipeline with branch-JAL-RET-prediction (BHT: 4096 entries, RAS: 8 entries).
The stages are IF (Fetch) ID (Decode) EX (Execute) WB (WriteBack).
Memory access is not considered a stage in the design, but just another late result that takes multiple cycles like division.

Software toolchain includes libc with builtin RTOS named `_OS (underLineOS)` which implements preemptive multi-threading (SMP capable), sw-timer, mutex, semaphore, fifo.

## Get project:

	git clone https://github.com/fontamsoc/rvpu.git
	git -C rvpu/ submodule update --init

## Run application using verilator simulator

A number of sample applications are available under `rvpu/hw/rv32-sim/apps/` along with their prebuilt .hex file:

	coremark  donut  helloworld  isatests  isatests2  smp_pi  thrdtst  tinyraytracer

### > App coremark using 1 MHz clock frequency:

	cd rvpu/hw/rv32-sim/
	make clean sim run SRAM_INITFILE=apps/coremark/rv32/coremark.hex CLKFREQ=1000000
	cd -

Output:

	rm -rf obj_dir *.vcd *.log
	make TB=sim obj_dir/Vsim
	make[1]: Entering directory '/tmp/rvpu/hw/rv32-sim'
	verilator -cc --exe  \
			--x-assign 1 --x-initial-edge \
			--relative-includes -I.. -Wno-lint \
			-DCLKFREQ=1000000 \
			-DCPU_COUNT=1 \
			-DSRAM_INITFILE='"apps/coremark/rv32/coremark.hex"' \
			-DSRAM_KBSIZE="(256/*KB*/)" \
			sim.v sim.cpp &>>build.log
	make[1]: Leaving directory '/tmp/rvpu/hw/rv32-sim'
	Thu May  8 10:31:15 AM CDT 2025
	apps/coremark/rv32/coremark.hex loaded
	CoreMark @ 1000000 Hz
	2K performance run parameters for coremark.
	CoreMark Size    : 666
	Total ticks      : 14019280
	Total time (secs): 14.019280
	Iterations/Sec   : 2.853214
	Iterations       : 40
	Compiler version : GCC13.3.0
	Compiler flags   : -O3 -DPERFORMANCE_RUN=1
	Memory location  : STATIC
	seedcrc          : 0xe9f5
	[0]crclist       : 0xe714
	[0]crcmatrix     : 0x1fd7
	[0]crcstate      : 0x8e3a
	[0]crcfinal      : 0x65c5
	Correct operation validated. See readme.txt for run and reporting rules.
	CoreMark 1.0 : 2.853214 / GCC13.3.0 -O3 -DPERFORMANCE_RUN=1 / STATIC
	CoreMark done
	- /tmp/rvpu/hw/rvxx/sys.pu.v:227: Verilog $finish

	real    0m15.618s
	user    0m11.148s
	sys     0m4.447s

### > App smp_pi using 4 cores:

	cd rvpu/hw/rv32-sim/
	make clean sim run SRAM_INITFILE=apps/smp_pi/smp_pi.hex CPU_COUNT=4
	cd -

Output:

	rm -rf obj_dir *.vcd *.log
	make TB=sim obj_dir/Vsim
	make[1]: Entering directory '/tmp/rvpu/hw/rv32-sim'
	verilator -cc --exe  \
			--x-assign 1 --x-initial-edge \
			--relative-includes -I.. -Wno-lint \
			-DCLKFREQ=100000000 \
			-DCPU_COUNT=4 \
			-DSRAM_INITFILE='"apps/smp_pi/smp_pi.hex"' \
			-DSRAM_KBSIZE="(256/*KB*/)" \
			sim.v sim.cpp &>>build.log
	make[1]: Leaving directory '/tmp/rvpu/hw/rv32-sim'
	Thu May  8 10:35:13 AM CDT 2025
	apps/smp_pi/smp_pi.hex loaded
	Calculate first 240 digits of Pi independently by 16 threads.
	Pi value calculated by thread #0: 314159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316
	Pi value calculated by thread #1: 314159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316
	Pi value calculated by thread #2: 314159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316
	Pi value calculated by thread #3: 314159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316
	Pi value calculated by thread #4: 314159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316
	Pi value calculated by thread #5: 314159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316
	Pi value calculated by thread #6: 314159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316
	Pi value calculated by thread #7: 314159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316
	Pi value calculated by thread #8: 314159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316
	Pi value calculated by thread #9: 314159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316
	Pi value calculated by thread #10: 314159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316
	Pi value calculated by thread #11: 314159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316
	Pi value calculated by thread #12: 314159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316
	Pi value calculated by thread #13: 314159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316
	Pi value calculated by thread #14: 314159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316
	Pi value calculated by thread #15: 314159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196442881097566593344612847564823378678316
	All 16 threads executed by 4 cores in 81 ms
	- /tmp/rvpu/hw/rvxx/sys.pu.v:227: Verilog $finish

	real    0m17.259s
	user    0m14.486s
	sys     0m2.728s

### > App tinyraytracer using 4 cores:

	cd rvpu/hw/rv32-sim/
	make clean sim run SRAM_INITFILE=apps/tinyraytracer/tinyraytracer.hex CPU_COUNT=4
	cd -

Output:

[![asciicast](https://asciinema.org/a/GIQaF4uvYTIRVMu7CEvh4jsJf.svg)](https://asciinema.org/a/GIQaF4uvYTIRVMu7CEvh4jsJf)

### > App donut:

	cd rvpu/hw/rv32-sim/
	make clean sim run SRAM_INITFILE=apps/donut/donut.hex
	cd -

Output:

[![asciicast](https://asciinema.org/a/IDh0X0zmR3x50dskrxjresdN7.svg)](https://asciinema.org/a/IDh0X0zmR3x50dskrxjresdN7)

## Run application on FPGA

FPGA support is available under `rvpu/hw/`:

	rv32-artya7100  rv32-nexysa7100  rv32-orangecrab0285

The easiest FPGA support to work with is the `rv32-orangecrab0285` because its sram can be updated without rebuilding the bitstream:

	cd rvpu/hw/rv32-orangecrab0285/yosys/
	make updsram SRAM_INITFILE=../../rv32-sim/apps/coremark/rv32/coremark.hex
	# While holding the button on the OrangeCrab, plug it in;
	# it enters the bootloader and enables programming a new bitstream.
	make prog
	cd -

Set `SRAM_INITFILE` of `make updsram` to a different .hex to use a different application.

To rebuild the `rv32-orangecrab0285` bitstream, use `make clean all`.

The bitstreams for the xilinx FPGAs, such as `rv32-artya7100`, need to be built from their Vivado project file (ie: `rvpu/hw/rv32-artya7100/vivado2020/artya7100.xpr`).

## Build toolchain (or [download prebuilt](https://github.com/fontamsoc/rvpu/releases/latest/download/rvpu-toolchain-linux-x64.tar.xz) [![pu32-toolchain](https://github.com/fontamsoc/rvpu/actions/workflows/release.yml/badge.svg)](https://github.com/fontamsoc/rvpu/actions/workflows/release.yml))

Only needed in order to build software.

	sudo ln -snf /bin/bash /bin/sh
	mkdir rvpu-build && cd rvpu-build
	make -f ../rvpu/makefile clean all

The toolchain gets generated under `/opt/rvpu-toolchain/`.

### > Install Toolchain if using downloaded prebuilt

	sudo tar -xf rvpu-toolchain-linux-x64.tar.xz -C /opt/
	PATH="/opt/rvpu-toolchain/bin:${PATH}"
