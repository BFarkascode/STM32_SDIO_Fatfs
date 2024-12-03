# STM32_SDIO_Fatfs
Bare metal project to implement Fatfs using an SDIO interface on a STM32_F405RG Feather board.

## General description
Ohh boy…

Honestly, I do not have the habit to bail out from a challenge, but here, I almost abandoned the project. ST really has overdone itself with the SDIO peripheral with the clunky implementation, buggy CubeMx and horrible documentation (both for SDIO and for the Sdcard). It was a true nightmare to make it work. I hope other people will benefit from my suffering…

Anway, enough rambling, let’s start from the beginning, shall we?

I have already done multiple projects where I was relying on an SPI driven Sdcard to store data. It wasn’t the most amount of fun to make that work mostly due to the not-so-easy-to-figure-out Sd documentation, but all things considered, the code was done and had been working reliably ever since.

I recently hit a roadblock though in relying on that code. On the hardware side.

As it goes, I have started using an STM32F405RG Feather board from Adafruit in an ongoing project due space restriction. It seemed like a good idea since it is a great and compact STM32 devboard (no STLink in it though, just a heads up) with an Sdcard reader already integrated into it.

What I quickly realised though that this reader isn’t attached to the MCU using SPI but through something called SDIO – Secure Digital Input/Output. What on Earth is that? – I was wondering…

### SD(IO)MMC cards?!
Just a few words on what we are dealing with: SDIO is a peripheral that is specially made to communicate with memory cards. I am writing here “memory cards”, because it can – supposedly – communicate with Sdcards, SDIO cards and MMC cards. No, I haven’t typed the same thing twice, SDcards are NOT same as SDIO cards! They are different hardware elements with different drive mechanisms, despite the fact that they are all driven by an SDIO peripheral.

But I am getting ahead of myself. In reality, we will need to take a dive in the “other” part of the SD specifications document, the part that details how to communicate with a card when we aren’t using SPI. If one remembers, CMD[0] with CS kept HIGH was what initiated SPI mode on an Sdcard, here, we will be looking at what to do when CS is pulled LOW.

A quick jab, the SD specifications document isn’t any better in these sections, making it an extreme chore to figure out, what we need to communicate with the card. When one reads through the document, extreme care must be given on the type of card it is referring to, especially to avoid mixing up the driven demands for the different card types. What we need is “Sd memory card” and nothing else, even if the part is called “SDIO”. Confusing? You bet!

### SDIO peripheral
But enough complaining about the SD documentation, what can SDIO do for us? Well, it is an alternative drive to SPI with completely different pins. which should liberate an SPI for us when using an Sdcard. Mind, an SDIO connected reader won’t be SPI compatible on a GPIO level, so once we picked SDIO, we are stuck with it.

SDIO is not limited to 10 MHz like SPI and has a “parallel” feature, where it can theoretically push data transfer rates from under 10 MHz to around 100 MHz (4-bit wide bus with 25 MHz clocking).

On a first glance, it also just seems to be an automatic version of what we have already done using SPI where, instead of constructing the entire packets using token and then pushing them on the SPI bus, we merely need to feed the peripheral a CMD and an argument. No biggie.

For all this, it seems then that we aren’t that unlucky with being “forced” to use SDIO on the ST32F405 Feather board after all, right?
Well…

#### Clouds on the horizon
Even just a small amount of reading up on the SDIO indicated to me that I am up for a whole lot of hurt.

Checking forum threads, it seemed to me that I was dealing with a peripheral that was “gently forgotten” by ST some time ago. Just an FYI, most bug reports I came across were dating back to the early 2010’s with apparently still no solution provided for the issues ever since.

To make things worse, the CubeMx solution is officially bugged on the F4xx devices, meaning that it won’t work at all if we intended to set it up without doing knowing what we are doing, defeating the purpose of having HAL. (Just a note, the GPIOs should be set to medium speed with a pullup in everything except the SCK line to repair this particular bug. 4-wide bus will not work without manual intervention though so don’t activate that).

There are multiple reports of SDIO FIFO underrun conditions that have vague solutions only and questionable results (more on that later).

The SDIO code example to block read in the refman is also complete garbage and missing crucial details that must be respected in order to avoid the peripheral to time out.

This isn’t good…

#### Going technical
Let’s roll up our sleaves and take a look into the peripheral more deeply.

SDIO is an autonomous peripheral with its own PLL clocking (shared with the USB peripheral). It can be clocked directly form the PLL, making it run fully parallel to the main loop. This output will be the PLLQ output of the PLL. It must be set up separately in the RCC register for the PLL. Of note, since it is running on its own source, it is even more autonome than the DMA which runs on the AHB busses. This is probably the key feature of the SDIO and something that leads to some issues with clock domains (see below).

Returning the SDIO proper, As mentioned above, the data bus width can be changed during init, which means that at a given clock rate, we could get multiple times greater data transfer rate with a wider bus. At the start, the SDIO is always set to run as 1-bit bus, which technically makes it analogue to SPI. A normal SDHC memory card – the most of us have lying around – can only be driven at 4-bit at maximum, though 8-bit is also available on the peripheral. Of note, this 4-bit width must be both set on the peripheral as well as the card with the appropriate registers and commands.

Under the hood, SDIO is an interplay between two separate state machines, one for the “command bus” (command path state machine - CPSM) and one for the “data transfer bus” (data path state machine - DPSM). They are NOT clocked on the same source (CMD/CPSM is on AHB, DPSM on the PLLQ) and both must be set up for each command and reply properly  - which means reconfiguring the SDIO.CMD and the SDIO.DCTRL registers EVERY TIME (!). As standard Sd memory card has 4 different command types (see below) and depending on the command type, the busses might be active, not active or expecting a short response or a long response on either just the CMD line or the DAT line(s) or both.

The CMD line is separate from the DAT line and thus is not affected by the selected bus width on the DAT line.

After the state machines are set, they – and SDIO in general – are very much driven by the data that is coming through either the CMD line and/or the DAT line. This means that the size of any kind of data packet moving through the busses must be precisely set within the state machines, otherwise they won’t be looking for the CRCs at the end of the incoming/outgoing packet. With CRC failing, the SDIO will throw an error message and stop working.

The packet size for the DAT bus is set by the [6:4] bits in the DCTRL register, meaning that once that many bytes have been sent/received, the state machine will generate a CRC or look for one. If none is found – or the block size is not set properly – the SDIO will fail with a CRC error. In case of SDHC, the block size should be the same size as the Sdcard’s block size (512 bytes), though partial readout is possible with, for instance SDSD cards. (Just a note, the ACMD13 CMD also will generate a 64 byte packet but that is a register readout coming through the DAT line, not a block read from the Sdcard itself. If we want to read the response of that coming on the DAT line, the block size must be set as 64.)

DLEN is always a multiple of the block size and no transfer smaller than a block is possible. DLEN is always a multiple of a block in BYTES. DCOUNT is the same as DLEN just it counts towards “0” during a transfer. When DCOUNT reaches “0”, unlike in SPI mode where a STOP_TRANSMISSION (CMD12) command might be necessary to put the system back to idle, the SDIO and the card go “idle” as if a CMD12 has been sent. Sending an additional CMD12 when the card is already “idle” will block the SDIO since CMD12 will ONLY generate a response (and thus progress the SDIO) if the card is transferring data already – is in states “data” or “rcv”.

I wasn’t using DTIMER, but both DLEN and DTIMER must be written to before the DCTRL register.

SD memory cards can only be run in SDIO “block mode”. Period.

There is an SDIORESP register in the SDIO, which shows, what the last accepted CMD was within the SDIO – i.e., what was he last CMD that responded back to the MCU. This has nothing to do with the RESP[0..4] registers which store the card’s answer to a command. To make things more confusing, some CMDs send their data on the data bus instead, see ACMD13…

SDIO STA stores the status of the SDIO at the moment of the CMD being sent, not after the CMD has been accepted. As such, it is critically important to read out the Sdcard’s status register (CMD13) regularly when polling is used to control the card so as to have both the SDIO and the Sdcard being in the same state. Mind, this option can be extremely unreliable and interrupts are highly recommended to control the driver instead.

CMD13 is also a good way to check if the card is busy or not by reading out the card status bits from the command’s response (bits [12:9]).

The status register of the card will tell in bits [12:9] what the current state of the Sdcard is. The status register – since it is an R1 response - will be in the RESP1 register. (More on that later.)

The SDIO STA flags are only partly dynamic, meaning that not all of them will be reset by the peripheral. Again, in case polling is used on the SDIO STA flags, one must reset any flag that is set manually by writing to the appropriate ICR register bit. Again-again, this is not recommended, as I came to realise myself as well.

The SDIO has a 32-word sized FIFO which is actually the same FIFO as the USB’s. It is possible to see if there is something in it when the code is paused and the IDE writes out the memory position of the FIFO (0x40012c80 on the F405RG). Be aware that this memory section can’t be manually interfaced with using pointers (again, more on this below).

Card selection is done by using “RCA” numbers. These are dynamically allocated IDs for each card, generated by the SDIO host when CMD3 is sent to the cards. The RCA numbers are then used to designate, which card we are talking to (similarly to how CS works in SPI). Theoretically, multiple cards can be connected to the same SDIO peripheral this way. (Of note, I have not investigated this option and stayed with just one card.)

Lastly, we still – mostly - have the same limitations as we had using SPI. So, under 400 kHz clocking during init, then we can go higher; the block size in SDHC is strictly 512 (card can’t be written to less than 512 bytes); read and write commands ask for “block address”, not “byte address”; and in fast systems, time will be needed to change pages on the card when multi-write and multi-read is used. One notable exception on how it was using SPI is that multi-read and multi-write DO NOT need a CMD12 to stop.

#### Commands of importance
I will discuss the commands a bit since failing to match the state machines (both the CMD and the DAT one) to each CMD will lead to problems (mostly SDIO timeous). Of note, the two different command tables are not just a stylistic solution to make the Sd documentation easier to read, the CMDs we need when using SDIO are NOT behaving the exactly the same as the ones we had during SPI, even if the state machine for the card is roughly the same as it was the SPI driving.

Important to see that some CMDs demand that we feed them the addressed card’s RCA, while others run with a blank (i.e. 0x0) RCA. This should be very precisely set when we define the packets for each CMD. Not giving the expected RCA will make the CMD freeze the SDIO.

Anyway, as mentioned above, we have 4 types of CMDs (see from p99 in the SD document).

- BC: this is also using just the CMD line. BC CMDs are commands only with no response. The only CMDs in this type are CMD0 and CMD7 – this later only (!) when we deselect the card!
- BCR: only uses just the CMD line. Sends the command and expects a response on the CMD line. It can only be used BEFORE we select a card. The commands will have different responses, where CMD2’s will be the CSD register (same as CMD10) as R2, CMD3 will the RCA (with the card’s dynamic address in bit [31:16]) as R6, CMD8 (with an echo on the ARG sent to the card) as R7 and ACMD41 (with the OCR register) as R3.
- AC: this type uses the CMD line. Once the CMD is sent, there will be a response on the CMD line. These commands can only be used once the card has been selected using CMD7. The following CMDs will be in the AC type: CMD7 (R1b), CMD9 (R2), CMD10 (R2), CMD13(R1), CMD23(R1), CMD55(R1), ACMD6(R1)
- ADTR: uses the CMD line and the DAT line. Can be sent only after the card is selected. CMD response comes on the CMD, actual data register readout, on the DAT line. CMD response is always R1. Commands of this type: CMD17, CMD18, CMD24, CMD25, ACMD13

Just a note, CMD7 is both an AC and a BC command, depending on if we use it to select or deselect the card. It will be an AC during select, BC during deselect.

And Another note: ACMD13 DLEN is 64 bytes since the SCR register is 512 bits, not bytes!

And another-anther note: RESPCMD for some commands is “63”, NOT the actual CMD index. Such CMDs are ACMD41, CMD9 and CMD10.

#### Responses of importance
Above I was using already the “R”-s to designate the type of responses (see p116 in the Sd document). Here is what they mean:

- No reply – some commands have no replies.
- R1 – this is the card status in RESP1 (with no CRC included!). This is what they call as a “short reply”. It is 32 bits so it will only be in RESP1.
- R1b – same as R1 just the card stays busy afterwards. Card won’t accept any new CMDs until it stops being busy.
- R2 – CSD and CID register WITH (!!!) the CRC included. Be aware of this difference when trying to unravel the response you have in the RESP registers! R2 is 128 bits so it takes all four RESP registers.
- R3 – OCR register. Reply to ACMD41. It is 32 bits, RESP1 only.
- R6 – RCA response. Reply to CMD3. It is 32 bits, rESP1 only.
- R7 – this is only used for CMD8. In it we will have an echo for the argument we have given CMD8. It is 32-bits, RESP1 only.

Again, I can’t emphasize this enough but depending on what the card will do, the SDIO will need to be set before sending a command to expect or not expect a response and how long that response should be – and the FIFO to be set to send/receive any information on the DAT line (ACMD13) or data read (CMD17/CMD18) or write (CMD24/CMD25).

Also, responses usually do not include the CRC…except for R2, where they do…

### A few words on clock domains
Mind, I am shooting in the dark here, but from the experience I have using FPGAs and RTOS, I am rather convinced that most problems that arise using SDIO come from clock domain crossing.

Let me explain…

Clock domains are, as the name suggests, domains of clocking which clock their respective MCUs or, in this case, peripherals. They are not something that one comes across often when dealing with a singular single core MC since there everything is clocked on the main clock source, bet that a resonator on PLL. If everything is clocked from the same source, everything is in synch and the different peripherals can talk to each other in synch.

What happens though when we have multiple clock domains and thus the MCUs/peripherals are NOT in synch? Well, in that case – if they share the same memory – it can very much happen that one element clocking at a fast speed “overtakes” another slower element, writing to the memory while the slower is reading it. This will lead to corrupted data being read by the slower element.

To avoid this “race condition” (in RTOS) or “clock domain crossing” (in FPGAs), we use handshake mechanisms to synch the two elements or, alternatively, we use FIFOs (queues in RTOS) to allow the data time to be processed. Another alternative is to use asynchronous signals/interrupts between the two elements to force them to synch with each other.

Theoretically, we should be in the same clock domain with the SDIO as with the rest of the MCU main loop since we are always clocking from the HSI. In reality though, small delays emerge on all clock signals as we use and process them, thus a tiny delay could eventually be introduced on two clock lines that otherwise have the same source.

### Points of significant pain
I will just list here the general problems with the SDIO that I have encountered:

1) I think, the SDIO is simply not in synch with the main loop. Yes, a FIFO is there but it doesn’t do a handshake. As such, if we try to poll the STA flags of the SDIO, the status flags will just spew garbage data, something that is completely unreliable and impossible to use for timing the execution of the main loop.
2) Due to the SDIO running on a separate clock – like a DMA - when we put a breakpoint on a polling loop, we won’t be reading out the value of the register when the code execution reached the breakpoint, but when – some cycles afterwards – the SDIO settles down. This can be especially frustrating if we want to check the progression of the SDIO STA register or the Sdcard’s status during a transfer. For instance, I have never managed to see the card being in “prg” despite the fact that it actually happens. (The breakpoint on the polling always shows only “tran” since that is what comes after “prg”, effectively skipping the “prg”). For similar reasons, the FIFOCNT does not indicate anything useful to us since, if we want to read it out, we need a breakpoint which resets the FIFO due to timeout, putting it to “0”. All these issues make debugging the SDIO incredibly frustrating…
3) Clocking is limited but this is not communicated. Since the SDIO runs on its own PLL output, it is perfectly possible to clock it faster than the main loop or even the servicing DMA connected to the SDIO. I initially though that this would not be a major issue since both the DMA and the SDIO have FIFOs which should be helping out with any kind of handshake going on. Or, the compiler would throw an error. Unfortunately, that is not the case at all and you can easily clock the SDIO faster than the DMA without any kind of error message (hello, one source of recurring “underrun” errors). What I have found is that the best to clock the SDIO at half AHB speed.
4) peaking of FIFOs, the SDIO’s and the DMA’s aren’t playing well with each other. In other words, once the SDIO is activated, it immediately triggers a FIFO error in the DMA that MUST cleared or a FIFO error will be generated in the SDIO as well (hello, underrun). This is a known bug and must be averted manually.
5) FIFO setup is not discussed or discussed in ways that would work. The refman suggests that the FIFO on the DMA should be set without flow control and in direct mode. That does not work. We need flow control and direct mode removed with a 4-beat burst, probably to bridge any kind of conversion from 8-bits to 32-bits. (SDIO is a 32-bit peripheral).
6) The FIFO is only active if the data path state machine (DPSM) is active. It seems to me that the FIFO is driven by the TXACT and RXACT flags in the SDIO STA register, which flags are set ONLY by the DPSM when it is configured to be receiver/transmitter AND is being enabled. The DPSM, if not in the proper state, will simply reset the read and write pointers of the FIFO, effectively wiping it clean. This makes preloading the FIFO - or interact with it manually whatsoever – impossible.
7) The DPSM times out 64 SDISCK after it is enabled. This is a VERY short time. Thus, one must fully set up the card using CMDs and configure the DMA before the DPSM is activated. Failing to do so times out the SDIO, putting it into “idle”. No errors will be generated though, just you won’t be seeing any data coming or going from the card.
8) SDIO is extremely timing sensitive. Finding the right timing came down to trial and error for me.
9) Clock bypass (full speed SDIO SCK) did not work for me.
10) Power saving clock mode did not work for me.

#### Workarounds
My initial attempt to make the SDIO work had been to continuously poll the STA register and time the main loop execution on what the register shows. This, for reasons mentioned above, resulted in a solution that was constantly timing out (STA[2]), not receiving enough data (STA[3]) or doing underruns (STA[5]). The solution had been to abandon the polling and use only interrupts from the SDIO instead.

Many of the underruns were also repaired by this (i.e. introducing an asynch handshake between the SDIO and the main loop in the form of interrupts), but not all of them. As it goes, underruns in the SDIO occur when the FIFO is not loaded at the same speed as we read it out and it simply runs dry for the dreaded 64 SCK cycles, timing out the SDIO. Clocking the SDIO slower can help with that. Lastly, the FIFO may not be properly set within the DMA to funnel the right amount of data width to the SDIO FIFO for instance by forgetting to set the 4-burst mode in the DMA.FCR register.

## Previous relevant projects
I recommend giving a quick look over the SPI Sdcard project I have done before, just to get up to speed on the pitfalls using an Sdcard:
- SAMD21_SPI_SD_fatfs

I have also found the following blog to be useful describing SDIO problems and giving a hint over how to work around them:

- https://blog.frankvh.com/2011/09/04/stm32f2xx-sdio-sd-card-interface/
- https://blog.frankvh.com/2011/12/30/stm32f2xx-stm32f4xx-sdio-interface-part-2/

## To read
The Sd documentation, obviously:
- https://www.sdcard.org/downloads/pls/

It is also a good idea to keep the ST forums opened in case some other nastiness comes in the way. For instance, on underruns:
- https://community.st.com/t5/stm32-mcus-products/sdio-tx-underrun-because-of-dma-fifo-error/m-p/535603

## Particularities
Please note that we have some FIFO-on-FIFO action here: the DMA FIFO is loading the SDIO FIFO. I ended up using both DMAs connected to the SDIO for Tx and Rx, though probably this is not necessary.

Speaking of DMAs, setting up the FCR register is crucial for this to work. Problem is that it isn’t explained, why, nor were the examples I have found particularly helpful. In case data is being dropped during transfer, the likely culprit is the FCR register. It can also be the source of underruns. For me, the solution had been 4-burst, non-direct mode.

DMA needs to run in peripheral flow control mode, so no NDTR register is used. Transfer number will come from SDIO directly.

I have mostly avoided polling the STA flags since it can very easily freeze the code execution. Only exception are CMD0, ACMD41 and CMD77 where a response to the CMD may not come on first call or at all for that matter. After each polling, the polled flag is reset in the STA register.

I saw that sometimes the SDIO is activated, but data transfer does not start. This is in the errata for the SDIO but for a different mode, not the one we are using. Nevertheless, I suggest checking that the card is in “rcv” mode before we activate a multi-write. Mind, doing the same thing for “multi read” may lead to data timeout fault since it may take too long for the main loop to poll for this state after multi-read CMD18 is sent. The card does not wait for an acknowledgement once data is demanded from it, so putting the SDIO into reception mode as soon as possible after that command is sent is critical. Mind, it is CMD18 that puts the card into “data” mode and the mode cannot be engaged prior.

Similarly, we can have data timeout IF we have the DPSM enabled at the wrong place. For read, it should be before we send the read command to the card (the card will send the data packet immediately upon the CMD), for write, it should be after.

It is said that one can just look for the “busy” signal in RESP1, but, in reality, in case we are expecting an R1b response, having a CMD coming immediately after the command with the R1b is sent, the second command does not seem to work.

The card states (see p53 in the Sd documentation) of importance to us are “tran” – which comes as we select the card using CMD7, “data” – which is after one of he read commands (CMD17 or CMD18) are sent to the card, “rcv” – which is the state after write commands (CMD14 or CMD25) and “prg” which is technically the card’s busy state while it is writing data to itself. Polling the card states using CMD13 works well for “tran”, “data” and “rcv”. Attention must be paid though to not overdo the polling and time out the SDIO accidentally.

Unlike SPI where CMD12 was necessary to stop any multi-read and multi-write commands, within SDIO, this does not apply. CMD12 is there only for stream mode which is not available for SD memory cards. As such, while CMD18 and CMD25 on the Sdcard says that they MUST be aborted using CMD12, on the SDIO’s side, we only need to send CMD18 and CMD25 with CMD23 set properly (in the state machine of the Sdcard, we will have “operation complete” generated). Sending CMD12 to a card that is not in “rcv” or “data” mode will lead to CMD timeout.

Once we have an SD_RCA, we keep on using it to communicate with the card. RCA is a dynamic chip select, generated by CMD3.

We have two different CMD sending functions depending on if we need RCA – card is not selected yet – or we don’t – card is already selected using CMD7.

CMD7 is a bit particular since it has an R1 when selecting a card but has not response when deselecting it. I have dealt with this by introducing a “dummy” CMD77 token which is identical to CMD7 with the exception that it sets the SDIO to not expect a response. When we deselect the card, we will be sending this dummy token to distinguish between select CMD7 and deselect CMD7 (CMD77).

I regularly select/deselect the card to avoid accidentally sending it CMDs which should not be sent while the card is activated (CMD9 and CMD10, for instance). I have decided to do so since I am not sure, how FATfs is juggling the card. This should allow the card to be always ready for any command.

Always (!) set the SDIO.DCTRL to match EXACTLY the CMD we are sending the card, or we are going to timeout left right and centre.

Also, the SDIO – even if timed out – does not stop being enabled, meaning that changing the DCTRL “on the fly” will result in garbage CMDs sent to the card. Always (!) shut off the SDIO command section before setting the DCTRL register.

Lastly, as mentioned above many-many times, due to the synchronization issues, only asynchrony solutions can work to facilitate a handshake between the SDIO and the main loop. In other words, reliability can only be achieved if hardware interrupts are used instead of polling the STA flags. Of note, it is possible to use polling just the outcome will be shaky.

## User guide
The code is just a bare bones implementation of FatFs running on SDIO. In order to demonstrate it, I merely removed the wav file generator from the STM32_Dictaphone project I did some time ago and made that run on a loop for five times.
Running the code should result in five empty wav files (so no data in them, just the header).

The five files are removed at the start of the code to showcase all basic FatFs functions.

Of note, I should add some timeout to the various “while” gates in the code to improve robustness and to deal with any kind of Sdcard errors. I have omitted that since within my projects, there would be no possibility for the code to recover anyway.
Lastly, a double starting is necessary when the card is hot swapped and the setup is reset or the device is powered on the first time. I have noticed this already on multiple cards that the CMD55/ACMD41 init loop does not pass on the very first try. I am not sure, why that is. Anyway, I have added a Reboot function to the ACMD41 loop just in case we fail to pass that part.

Lastly-lastly, word of advice: I have not tested the SDIO with RTOS, but, considering how sensitive both systems are to interrupts, they may not play nicely with each other. Apply extreme caution if the code is being implemented alongside an RTOS!

## Conclusion
This was a significantly more laborious project than what I estimated. I though it would simply be just a direct copy of the SPI driving in a simpler wrapping but turned out to be a complex wild goose chase to find out, what the hell is going on. At any rate, the code seems to run reliably and at a significantly greater speed compared to SPI, which is a great benefit at the end of the day.

