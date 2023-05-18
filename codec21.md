# CODEC21 Environment Friendly Low Latency Media Codec (preliminary information)

Patented as US 11,638,040 B2. Patent and trademark rights reserved.

Copyright Licensed with Creative Commons CC0. Copyright© Schmied Enterprises LLC 2020-2021. 

To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.

## Abstract

This is a low latency audio, video, and graphics codec for remote applications.
It leverages a dedicated channel providing predictable bandwidth.
It attaches the presentation to the audio omitting timestamps.

It is designed to compete with the patent packages of H.264, H.265.

It requires a simple implementation without session or frame headers to minimize support and testing costs.
It is designed to run on microcontrollers without the requirement of floating point instructions.

It is designed to have minimal interdependence between parts of the stream, so that encoding and decoding can be highly parallel.
It is designed with low complexity and power use making it ideal for green applications.

All these make it suitable for scalable small businesses to use with remote desktops, screen connectors (USB3 over HDMI), local broadcast, embedded screens, remote gaming, remote driving, internet browsing.
It is simple, and it reduces the risks of legal, hardware and software security problems and expenses.

## Technical details

### Fix ratio objective

It is supposed to compress the original image to one fifth or less.
The number 20 in the name reflects the **20% nominal compressed stream bitrate** compared to the raw pixels.
The solution keeps the compression ratio stable.
It is capped at this level by the encoder.

Engineers can predict the workload and adjust network quotas to the nominal rate.
Retail and consumer optical networks will spread more allowing higher fixed bandwidths at lower costs.
Retail use can be tailored to use this codec efficiently.
Simple codecs like this one may ensure competitiveness of the companies implementing them.

Historically bandwidth and storage were expensive generating the demand for codecs such as H.264, H.265.
Recent chip shortages suggest that **compute costs were less flexible** generating a demand for codec21 and green low latency solutions.
This makes it less useful for archival and public broadcast, where the channel incurs higher costs.
These applications are expected to converge to lower cost in the future, too.
This is especially true to initial project engineering and management.

Office applications are the suggested usage market with rare but quick updates.
A typical usage is paging on the results of a budget query.
Remoting provides the **highest level of integrity** with easy verification.

It compresses the signal and does not anticipate patterns making the image sharper.
It retains sharp changes and lines better than H.264 or JPEG making it better for office use.

### Bandwidth requirement

Animation is supported with coarse quick updates and progressive refinement.
However, high-end games are not the main usage scenario of the scope of this document.
Higher resolutions can be achieved by multiplying the channel.
If a 1Kx1K resolution is supported by a device, hardware vendors can simply add 16 channels to support 4Kx4K without any expensive software changes or codec profile engineering.

This is a low profile codec as an alternative to traditional H.264-H.265.
Codec21 does not require **specialized hardware of H.264 and H.265** improving support and integrity.

The target audience are users of office applications. This assumes a 200 Mbps@15ms
wired connection per human user.
**25 Mbps is reserved** by codec21. Less than 3% of the
connection is reserved for uncompressed 200kHz 16 bit digitally encoded pulse audio
of 3.3 Mbps.

Video is reserved 20 Mbps that is sufficient to encode an uncompressed
100 Mbps stream with codec21.
It is important to note that these are office applications with relatively rare updates and long focus time.

It is also important that as network bandwidth gets higher, the usage scenarios enabled by lower latencies become better by implementing companies.
It is not the case for H.264 and H.265. Higher bandwidth requires higher processing requirement there due to the expensive color space, wavelet, and convolutional transformations.
This dissipates more heat, requires more chip power.
It also requires a more complex high density silicon to sustain the latency.
It is not that Codec21 is a better compression algorithm.
Codec21 engineering profile allows flexibility to reduce compute leveraging increasing channel bandwidths.

This allows 64K pixels per true color frame. This is
VHS quality at 60Hz, CD quality at 16Hz, HD quality at 4Hz and 4K quality 1Hz.
Better compression of quick motion video can be achieved by subsampling the image grabbing 2, 4, 8, or 16 pixel bursts, and refine in progressive updates.
This can achieve a **nominal 5% and 1.25%** compressed media ratio.
This is already comparable to the traditional codecs for movies.

Obviously, the user does not update the entire frame when word editing, 
and progressive updates smoothen any sudden changes of the stream like chat applications.

```
20 mbps compressed = 20971520 bps =
               873,813 pps 
Ratio 20%    4,369,067 pps
Ratio 5%    17,476,267 pps
Ratio 1.25% 69,905,066 pps
```

Codec21 is **progressive** with differential encoding. This progressively increases the
quality and lowers the latency for real life office applications with reasonable
power usage. A button press triggers just a change of few thousand pixels. The
roundtrip can be as low as 16 ms for the channel plus 16 ms for the server delay.
Future applications of codec21 may work over a 40 Mbps 40 ms latency LTE
connection this way. All this assumes real time encoding and decoding with simple
primitives and low embedded resolutions.

Codec21 is **scalable** requiring no restrictions on resolution. This means that an 5Gb
USB3 or HDMI channel can bring 16x16 times the original bandwidth providing a true color lossless
4096x4096@60 4K stream with the 5x compression ratio of codec21.

Lower bandwidth channels can be duplicated, and higher bandwidth channels can leverage the code without changes due to the header-less implementation that we promote.
Omitting any negotiating headers reduce early engineering time significantly, when the first prototype is made reliable.

```
5 gbps USB3 compressed = 5,368,709,120 bps =
             223,696,213 pps 
Ratio 20%  1,118,481,067 pps 4K@120Hz
```

There is no engineering overhead. You do not even need to change a header to achieve this.
The key of the feature is supportability that makes it useful for applications with few units.

The codec allows **parallel processing** reducing the latency.
High compression rates required by streaming or archiving may still need 
other traditional H.264 or H.265 and a bit more expensive hardware support.
Traditional industry codecs are still better option for bandwidth intensive applications,
if bigger compute and real time requirements can be supported by extra investment.

### Compression method

The compression ratio of 5x down to 20%-21% comes from two factors.

Graphics that is not color rich may use lookup tables for coarse values using **two bit indices in palettes of four pixels** per pixel
instead of 24 bits. It can be refined with residuals in progressive updates. Text can go even
further using just one bit and two major colors for the first update.

Real world images may rely on a **palette** of 256 for flat and transient
surfaces and residuals in subsequent updates to achieve the ratio.
However, this will cause quality degradation seen by GIF images.
It also increases encoding complexity and reduces the way decompression can be run in parallel.
We advise using two, four or sixteen pixel palettes.

Images with details can transfer **2 bits per color component** at a time.
Four progressive updates may refine it in subsequent frames reducing 
the bit rate by 4x in each update as a minimum. Flat or dark surfaces improve the ratio.

Real world images are better compressed with linear slope **regression** encoder and an **interpolation decoder** and that requires a length and a closing pixel to define the slope.
The interpolation technique still does not require floating point instructions for embedded and microcontroller applications.
The ratio is at least 20% if more than 8 pixels are encrypted at a time.

Office applications require low **latency for interactions** like typing, which typically require changes of a small portion of the frame.
This can reduce the latency further by a magnitude (10x).

Our codec effectively becomes a fusion of GIF, PNG, and JPEG-XS with a good upper limit predicted by PNG compression results based on our experiments.

### Power

The codec design embraces low power usage. Power usage is usually proportional to the number of bit flips in digital logic.
**Complex floating point calculations and high frequencies** reduce energy efficiency.
The codec21 embraces simple single computations for each pixel reducing the theoretical power use.

This will improve **battery life and battery size**. It also reduces the size of factories and number of workers manufacturing these.
Optical networks will also reduce power use of devices implementing Codec21 as a side effect.

Most codecs use floating point or fixed point operations that needs expensive GPUs.
It is also cumbersome and expensive to debug errors and hacks.
Employees, who debug have huge carbon footprint, and their energy can be used to research green technologies.

The simplicity of the implementation allows **smaller batteries**, predictable power usage, and the availability of lower end user electronics.
The general rule is that the power usage of components decrease by reducing bit flips caused by excess calculations.

We have a cost-efficient, easy to implement codec for **green and low 
power** applications as a result.

### Latency

A dedicated channel is assumed for real time low latency traffic. This reduces
jitter. No timestamps are used.
The length of the audio specifies the length of the frame.

Simplicity still allows use for archival. The timing of the audio channel should be
sufficient for this purpose.
Any buffering causes quality degradation and it increases the time and cost
of engineering support.
This is an issue for small scale applications.
We propose uncompressed audio to reduce any decompression delays to zero.
Bandwidth in the 2020s should be sufficient to do so.
Empty audio frames can use a differential or run-length encoded dummy audio channel
of a few bytes to set the length of the frame.

The codec algorithm ensures that there is no dependency between lines.
Also, lines can be split into arbitrary amount of blocks as long as the theoretical limit of 20% compression ratio is achieved.
This may help to increase the hardware decoding and encoding blocks in order to reduce latency.
Power usage should be proportional to the amount of work, if idle encoder and decoder blocks are omitted from the stream.
This allows minimal impact of the level of parallelism on the system.

Below are the expected coarse and refined latencies and latency of small interactive changes with the 25mbps channel reservation.
```
TODO
Height   Coarse width  Refined width  Coarse latency  Refined latency  Typing latency 
2048pix  10700pix      2100pix        190 ms          960 ms           0.75 ms
1080pix  20200pix      4000pix        50 ms           270 ms           0.75 ms
720pix   30300pix      6100pix        20 ms           200 ms           0.75 ms
480pix   45500pix      9100pix        10 ms           50 ms            0.75 ms
200pix   109200pix     21800pix       2 ms            10 ms            0.75 ms
```

### Eventually Lossless Codec

The main attribute of codec21 is that it is does not lose information over time.
Each frame may have some difference from the original image,
however subsequent frames eventually compensate for the differences.
This is useful to ignore transient noise that is not noticeable on high frame rates by the human eye.
Quickly changing full frames usually does not give
enough time for the user to accommodate to the details.

The encoder can compensate for sudden changes over time and within the frame with low pass filters.
This eliminates artifacts of low bandwidth issues of H.264.

### Audio

200 KHz audio should provide superior quality over existing standards with low
overhead. It is just a portion of the entire frame and decompressed transmission
ensures minimal delay especially for interactive applications.
Audio compression is discouraged.
Lossful compression of audio is a sign of inadequate input quality with few
frequency components or a sampling rate too low. Loss in compression causes
noticeable quality degradation, a key differentiator for our user base.

```
Audio Sampling rate			200 000	Hz
Bytes per sample			2	bytes
Time of sample				5	us
1ms transfer time (100Mbit)	        31	us over 100mbps channel
1 ms buffer size (1000Hz)		400	bytes 1ms transfers in 0.033ms
10 ms buffer size (100Hz)		4000	bytes 10ms transfers in 0.33ms
16 ms buffer size (60Hz)		6700	bytes 16ms transfers in 0.5ms
Audio bandwidth				3.3	Mbps
Audio/Video bandwidth			22	Mbps
Audio badwidth ratio			15	%
```

The audio part is 200kHz 16-bit sampled Pulse Code Modulated data.
If we use a 100 Hz nominal frame rate, then it is 4000 bytes of audio for a frame of 10 ms that transfers in less than 1 ms over a 100 mbps channel providing the low latency required.
Pulse code modulation basically transmits the sampled level of the audio signal at every cycle.

This is less than a single row of uncompressed video.
It is uncompressed as it is normally just a fraction of the video part. It has to be
decoded fast and cheap, so we do not use algorithms like MP3 designed for audio CDs.
Audio is encoded before video.
Compressing audio more would not affect the entire bandwidth with video much.

The lack of compression gives the lowest latency possible. We may compress trivial scenarios, like silence.
This is true even if the network latency has a jitter of +-20%
Multiple channels may be supported by multiple streams.
Traditional MP3 is still better for archival and broadcast over the air where the bandwidth matters.
An example is if it is paid by the usage (paid by GB used), and not by the channel (unlimited contract).
We can still use the same **video signal encoding algorithms for audio**, as long as they do not incur losses.


This is a low latency real time codec for remote desktop applications.
**Audio provides the synchronization**.
Audio is synchronized with the first display of the frame.
Once the frame buffer is filled, we wait for the end
of the last audio block and swap the frame, starting the new audio block.
Any subsequent progressive video updates come while
audio is already running.

There is no fixed syncronization rate.
We get an immediate reaction to a button press on a stale image submitting a
small portion of image and sound together making the effective frame rate 2x or even
4x of the nominal 15Hz, 30Hz or 60Hz to real 60Hz, 120Hz or 240Hz.
The whole audio buffer can be transmitted at the beginning of the frame.
Transmission errors may affect the video as glitches, buffering or delaying should not be used.

Rather than buffering, quality of service can be achieved due to the low percentage of bandwidth
requirement of 20 Mbps compared to the suggested 100 Mbps channel bandwidth.
This is a significant difference compared to other applications.
We do not saturate the channel.
All buffering should be limited to waiting until the previous audio buffer was played.
This is allowed due to the high bandwidth requirement and the low audio to video bandwidth ratio.

Not saturating the channel to achieve latency is a very important aspect of this codec.

```
Net: |--audio (abc)---|---video coarse (bcd)----|--video progressive refinement--|--video progressive refinement--|
Audio playing: ...----previous audio-------------------|-------current audio abc---------------------...
Video shown:                                           |video swap to (bcd)      | video swap to progressive...
```

### Base frequency and resolution

We do not have a nominal frame rate. The length of the audio sample specifies the
length of the frame. It does not have a negotiation header to specify resolution or
frame rate. This reduces support and testing costs.

Small changes may be sent immediately if sufficient bandwidth is given with no delay.
This is very usefult to support typing in real time.

We suggest using low pass filtering on video for convenience and compression on the encoder side.
Also, we suggest no filtering on audio to support a diverse user base including high frequency voice.
This will reduce flickering, artifacts and the unnecessary bandwidth increase of noisy images.
Higher frame rates may be filtered in the decoder code of the display.

### Viewport and client buffering

The codec assumes a very simple client and server code. This helps to reduce
the fixed development costs. It also reduces the variable cost of client hardware.
It also reduces support engineering and security audit costs and risks reducing the carbon footprint even more.

Viewport definition:
 - The input stream is expanded into client memory
 - Client memory contains a rectangle bigger than the actual displayed viewport 
 - The image buffer can hold an image larger than the size of the client display.
 - The viewport can be implemented with a standard double buffering technique
reading from one and writing to the other buffer.
 - The buffers are swapped when the audio of the read buffer stops playing and the
first frame of the next buffer is plays.
 - A special case is channel reset, if audio is interrupted and truncated on user input.
 - We still display the frame buffer once the last audio frame finished, ran out.
 - The new image is displayed and the new audio starts playing when swapping. The image 
is gradually updated with progressive updates that are swapped in later.
 - This is an application layer protocol. Behavior on network errors is undefined. Reliability is the responsibility of lower network layers. This brings flexibility.
 - The audio buffer containing the uncompressed 200 KHz PCM sample is stored first.
 - Suggested audio sample buffer size for music playback: 4096 bytes (10ms@100Hz@100mbps)
 - Suggested audio sample buffer size for low latency chat: 512 byte (1.5ms@1000Hz@100mbps).
 - The invisible palette of pixels may come anytime. This is a variable length array of 24
bit pixels. The size is up to 256 pixels long like GIF but 2, 4, and 16 pixel palettes are encouraged based on our experimentation.
 - No resolution is provided by the encoder since headers are non existent to reduce engineering cost.
 - The client can opt for an optimal resolution to be sent in the URL or filename:
https://c20.remoting.example.com/600x800 file://stream600x800.c20
 - Each pixel should represent a 1/60 degree viewing angle to maintain text quality.
 - The viewport is set by client driven actions like scrolling.
 - The viewing angle constraint may require the client to implement paging
(preferred), scrolling, or cropping lines of the viewport.
   This allows the presentation of documents as well.
 - The default cursor location is (0,0).
 - The default video view port is from the default cursor with the standard viewing
angle.
 - The default width is the widest span of pixels right from the cursor that meets
the 1/60 angle constraint.
 - The default height is the widest span of pixels down from the cursor that meets
the 1/60 angle constraint.
 - The default width and height of each frame is the width of the previous frame.
 - Bigger images may follow into new frame buffers.
 - Zooming and cropping is possible by the client independently to improve latency.
The client may crop to reflect the client native resolution.
 - The client may zoom out images that do not contain text or images with text as a
thumbnail.
 - Any zoomed client thumbnails should keep original average brightness and avoid
interlacing or coarse subsampling.
 - Use high contrast interpolation. Two pixel flat surface meeting another two pixel
flats surface should have a sharp edge.
 - Use linear interpolation. We should use a linear interpolation on dull edges.
 - The server may change the resolution on the fly in any image regardless.
 - The session end resets the entire client buffer.

### Sections

These are the section primitives of the codec.
 - Session: Session is a set of frames. Resetting a session means losing connection.
All local cache is discarded. Session reset has no actual codec command.
   It is defined by the carrier. 
   We start a new black frame of RGB (0, 0, 0) and silent audio. A server may send a
reference frame (traditionally called an I frame) by eliminating references to the
previous frames and overwriting all pixels.
 - Frame: A frame is a physical image and the matching audio sample.
 When we start the frame it resets the cursor to the (0,0)
reference point for subsequent data. We expect to reference and reuse the pixels of 
the last frame. Frames are eventually lossless blending with latter frames with smaller changes gradually.
 - Section/Row: Row is a horizontal line pixels. Sections/Rows do not refer to each other
and rows may be split into multiple buffers to allow parallel processing.
The client can scan the buffer and expand each section/row asynchronously for low latency. 
End of row N jumps to the (0, N+1) reference point copying the rest of the row as it was in the previous frame.
 - Pixel: Pixel is a single point in the image. It is true color RGB24. It
represents 1/60 of a degree of viewing angle.

### Update process

Each frame receives the audio buffer first then the image data.
The audio buffer is stored until the image data is decompressed fully or progressively based on the last frame.

Once the image buffer is received, we wait for the last audio buffer to finish playing and we start the current audio buffer.
The client swaps it into the visible frame buffer.

When a subsequent progressive section is updated, it is swapped with the visible previous section of the current frame.
The next frame is not swapped until the audio track of the current frame is
finished.

Note: A frame may be swapped if the audio is silent in a low latency environment.
Note: A session reset may be used to interrupt the current audio and play a new one.

## Protocol

The compressed stream is composed of the following primitives.
 - Array byte `<00 - End of Frame marker | NUM>00 - Array size>`
 - Marker byte `<00-0F>`
 - Data array: `<N pixels specified by the marker byte method>`

The codec has different patterns. Each pattern is designed with simplicity in mind reducing implementation costs.

### Frame divider marker

 - **EOF/NOP (00)** The reserved marker code for end of frame. Multiple 00 codes after
EOF are reserved for padding dummy streams or to fill up buffers.
 * Example end of frame:
   `EOF(00)`
 * Example padding without effect:
   `EOF(00) NOP(00) NOP(00) NOP(00) NOP(00) NOP(00)
NOP(00)`

### Array size

 - **NUM (01-FF)** Array: Each array specifies the number of pixels (N) that follow.
Multiple chunks may be used for bigger arrays helping with parallelism.

   The overhead of the array size and marker is ~1% in case when the pixel count is bigger than 64.

   The overhead of the array size and marker is ~2% in case when the pixel count is bigger than 32.

   These fit into our compression goal of 20%

 * Example array of 5 rows skipped and kept from previous frame:
   `EOF(00) NUM(05) ROW(0D) EOF(00)`

### Audio markers

 * **Default audio of N\*640us** Audio markers are valid after the frame marker EOF(00) before the first row marker ROW(0D).
   Example special case silent audio buffer of 16 milliseconds
   
   `FRM(00) ROW(0D) ...` expanding to `00 00 00 ...`


 * **SIL (00)** marker specifies a special case of a silent channel, and N just sets the length of the frame in N * 640us as if the count of empty 256 byte buffers.

   Example silent audio buffer of 16 milliseconds 640us*25=16ms (25=0x19)

   `FRM(00) NUM(0x19) SIL(00) ROW(0D) ...` expanding to `00 00 00 ...`


 * **RAW (0F)** This is the marker code to send N pieces of raw audio samples of 256 bytes.
   We use 200 KHz 16-bit pulse code modulated audio.
   Each sample consists of 128 16 bit word 5us audio samples making up ~640 microseconds (128*5us) in total.
   Subsequent samples are copied and played after each other.
   The frame image is swapped when the audio sample starts playing.

   Example 16 milliseconds of raw 200kHz@16bit audio 640us*25=16ms (25=0x19):

   `FRM(00) NUM(0x19) RAW(0F) 00 01 02 03 ... ROW(0D) ` expanding to `00 01 02 03 ...`
   
### Pixel markers

 * **ROW (0D)** The marker code to begin a new image row.

   Example new line:
  `EOF(00) ... NUM(01) ROW(0D) ... <pixels>`

   Example array of 4 rows skipped and kept from the previous frame:
  `EOF(00) NUM(05) ROW(0D) EOF(00)`

   Example skip 128 rows positioning into the middle of the frame:
  `EOF(00) NUM(80) ROW(0D) NUM(5) ... <pixels>`


- **LIN (0E)**: Generate N pixels with a linear interpolation from two and override the existing value.
  Example ramp from black to gray: `NUM(1) RAW(0F) RGB(000000) NUM(80) LIN(0E) RGB(808080)`


 * **RAW (0F)**: The marker code pixel values specify 24-bit RGB pixels overriding the existing one.
  Example five pixels: N(05) RAW(0F) FF00FF 803070 FF00FE 803060 000000


 * **TRN (00)** Transparent pixel array or skip N pixels
   00 is a special case for transparent pixels keeping the pixel of the last frame.

   Example for moving the cursor right 5 pixels skipping them, keeping them from the last frame: `NUM(5) DIF(00)`


- **IDX (01, 02, 03)** Palette and index array of 1, 2, 4, 8 bits of 2, 4, 16, 256 pixels.
  N 24-bit pixels follow then N indexes compacted into full bytes.
  A wider lookup table is unnecessary due to progressive updates.
  Indexes follow into the palette lookup table. Each palette item is 24 bits wide.

  IDX(01)=2 pixels wide palette, 1 bit per pixel index

  IDX(02)=4 pixels wide palette, 2 bits per pixel index

  IDX(03)=16 pixels wide palette, 4 bits per pixel index

  Example array of 80 pixels 1 bit index will use 80 bits, 10 bytes: `NUM(50) IDX(01) 000000 FFFFFF 34 56
  78 90 34 56 78 90 34 56`


 * **DIF (04, 05, 06, 07)**: Raw quantized pixel values specify 24-bit RGB pixels sliced.
   
   04 means pixel bits 7-6 sent for each channel, 6 bits per pixel overall. The existing pixel is overwritten.

   05 means pixel bits 5-4 sent for each channel, 6 bits per pixel overall. Each pixel is added to the existing value.

   06 means pixel bits 3-2 sent for each channel, 6 bits per pixel overall. Each pixel is added to the existing value.

   07 means pixel bits 0-1 sent for each channel, 6 bits per pixel overall. Each pixel is added to the existing value.

   Bits 7-6 with marker 04 may actually be too dark and rough.
   It can be replaced with IDX(02) marker and four bright pixels.
   Repetition and previous pixel reference can be simluated with the LIN marker.

   Example fine adjustments of 12 pixels of 24 bits, 3 bytes: `NUM(06) DIF(05) 08 06 05`

### Example stream of 2 frames

Audio data is 200 KHz 16-bit PCM. Palette Lookup Table and Image data is
24-bit RGB already multiplied with Alpha.

```
Frame start: |--EOF--|
Audio: |--Array byte--|--Raw audio marker--|--Data byte--|--Data byte--|...|--Data byte--|--ROW--|
Positioning: |--Array byte--|--ROW--|
Pixel row: |--Array byte--|--Pattern byte--|--Data byte--|--Data byte--|...|--Data byte--| |--Array byte--|--Pattern byte--|--Data byte--|--Data byte--|...|--Data byte--|--ROW--|
Pixel row: |--Array byte--|--Pattern byte--|--Data byte--|--Data byte--|...|--Data byte--| |--Array byte--|--Pattern byte--|--Data byte--|--Data byte--|...|--Data byte--|--ROW--|
Frame start: |--EOF--|
Audio: |--Array byte--|--Silent audio marker--|--ROW--|
Positioning: |--Array byte--|--Array byte--|--ROW--|
Pixel row: |--Array byte--|--Pattern byte--|--Data byte--|--Data byte--|...|--Data byte--| |--Array byte--|--Pattern byte--|--Data byte--|--Data byte--|...|--Data byte--|--ROW--|
Pixel row: |--Array byte--|--Pattern byte--|--Data byte--|--Data byte--|...|--Data byte--| |--Array byte--|--Pattern byte--|--Data byte--|--Data byte--|...|--Data byte--|--ROW--|
Frame start: |--EOF--|
```

### Discussion of Design Considerations

 - The computer science history embraced resource usage and bandwidth resulting in the standards like MPEG, MP3, or H.264, H.265.
   Quality, power use and low latency are more important in the age of **battery driven, higher bandwidth, guaranteed QoS** devices.
 - Codec consumes just a **portion of the bandwidth** available, so constant factors like YUV are not significant.
   This is especially true considering the memory bandwidth requirement of YUV conversion.
 - Parallelism requires no connection **across lines**. Vertical correlations usually imply horizontal correlations making the impact of horizontal compression adequate.
   Leveraging vertical correlations has just an engineering overhead, and a parallel scale out cap.
   8x8, 16x16 blocks are unnecessary overhead of traditional codecs like H.264. Refer to JPEG-XS for a similar implementation.
 - **Simple bitstream** is easy to debug by hand helping to reduce long term maintenance and security audit costs.
   Observability and low fixed engineering cost is a key differentiator.
 - Because of this we **do not use traditional techniques** like motion detection,
   wavelet transformation, chroma subsampling, and convolutional transformation
   as they are very power hungry.
   We concentrate on the most efficient and common algorithms like previous frame reference, linear interpolation, lookup tables, and quantizing.
 - Codec21 preserves **sharp edges** better making it more suitable for office use than H.264.
 - Traditional **GIF** has a palette of 256 pixels with 8 bit indices.
   Codec21 has an at most **16 pixel 4 bit index** local palette enabling better parallelism and half the index arrays.
   This allows 512 or more pixel values making the picture more realistic with **less noise**.
 - Traditional **PNG uses deflate and repetition** that is difficult to make parallel and extra low latency.
   Deflate algorithm is also heavy on computation due to the excess memory and cache access.
   One of the main weakness of Raspberry Pi is the slow unzipping time due to inefficient caching.
 - Traditional **JPEG and MPEG rely on waveforms and block correlations** requiring higher memory use, compute and power use. 
 - **Codec21 has a linear O(n) encoding** algorithm for palette lookup, linear interpolation, and quantization making it efficient for embedded applications
 - An encoder specific noise threshold can avoid stripes due to the local palette with minimal overhead.
 - We **do not use deflate** or any other enthropy encoding. The reason is simply to optimize for low latency.
   Deflate requires excess memory and excess compute in exchange for about additional 50% compression ratio.
   This may be acceptable for certain implementations, but it may prevent the use of microcontrollers, so we decided against it.
 - Another reason not using deflate is that we have a **fixed bandwidth saturation requirement** of 25 Mbps of 100 Mbps
   Bandwidth is not an issue. We optimize for low compute and memory use.
   Entropy encoding standards like deflate shorten some code words but lengthen others.
   Individual implementations may apply it in any case, since we saw overall effects of the quality comparable to and the size better than PNG.
 - Subsampling of the image as 2, 4, 8, 16 pixel bursts similar to wavelet transformations may give a quick latency enhancement for large frame updates (I frames)
   **Subsampling** can be added separately on the input.
 - Security like TLS is not discussed. It may add addtitional latency, so it may be omitted for public broadcasting.
   TLS may be replaced by **signing algorithms** that disregard privacy but ensure integrity.
 - A single test image is used, but it is sufficient, since it contains all regular patterns simulating more complicated tests.
   This reduces **test engineering overhead**. This is the power of the simple design, that we can assess the test set required as a function of the codec code complexity.
 - The core codec code should be kept around **1000 lines** that can be maintained by **1/5 developer year** of an experienced engineer. (McEnery, 2020)
   It is a good idea not to have code that is not maintained to keep stability and integrity.
 - We choose the local compression algorithms based on (1) accuracy, (2) whether it meets the 20% compression goal,
   (3) whether it compresses better, (4) whether it spans across bigger blocks.
 - We **do not do motion detection** to compress, since, when motion compensation is needed, it is enforced on a bigger moving block.
   Bigger moving blocks imply a strong intra-frame correlation that our algorithm already leverages.
   Moving images are rarely random noise, and only large patches can be recognized by the eye.
   This design decision is possible due to the fixed ratio requirement versus the best ratio of archival codecs.
   Motion detection is also a minor security risk.

All this is allowed by the decades of experience technology collected.
Former theories and assumptions of scientists are proven by empirical evidence for managers, analysts and engineers to date.

## References

[Reference: McEnery, Sage. (Jul 18, 2020). How much computer code has been written?. Medium.](https://medium.com/modern-stack/how-much-computer-code-has-been-written-c8c03100f459)

Contact: hq@schmied.us | Schmied Enterprises LLC 