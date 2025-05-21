# CODEC21

The project is an audio visual codec implementation. Codecs encode and decode media into digital formats usually focusing on a single goal like bandwidth, size, latency, power use, processor use, or special network topologies.

This implementation is focusing on flexibility. It allows AI models to refine the codebase enabling low power use, a rare feature among codecs.

We are also very simple making the lowest cost devices possible. Manufacturers can get the bang for the buck skewing the BOM to better and bigger display panels over industrial PCs behind them increasing customer value. Defense companies can focus on lower cost working assets rather than a long term investment making the BOM cheaper allowing bigger ranges or heavier payloads. Education can leverage the codebase with our flexible patent licensing requiring payments only on EBITDA.

First you need to download and prepare some test files. We use PNGs read from the disk directly reducing the decoding overhead of H.264, H.265. This allows more accurate benchmarking of our own codebase.

Each code file is less than 1000 lines making them suitable for tweaking and optimizing with AI. AI is very useful on the encoder side optimizing for workloads and different hyperparameters. The results can be mindblowing.

Security Professionals: If you use AI to generate the encoder and decoder code only and you ship and distribute the audited code, it can be suitable for enterprise and defense usecases.

We also include many versions, so that they can be used for AI training setting the bootstrapping boundaries.

The flexibility and simplicity of the open source codebase targets a premier ecosystem of leading management consulting companies.

```
cat generate_png.c | grep gcc | bash
```

Run a standalone version of the codec encoding and decoding a stream of png files in the same process presenting on the screen in WSL2.

```
cat standalone.c | grep gcc | bash
```

Run a Unix script and view the output in a webpage.

Upload the viewer page just to try it works.

```
cat index.html | curl -X 'PUT' --data-binary @- 'https://theme25.com?format=https://theme25.com%25s?Content-Type=text/html'
```

Render and stream the console to a webpage using a series of PNG files. TODO: Use CODEC21 instead of PNG.

```
cat script_to_png.c | grep gcc | bash
```

Open the html in the browser like below. The secure link can be found in the script output.

```
https://theme25.com/826c3607194640b63f9beb42a6ed182e76700b1c8d120049a588f6ffc87ececd.tig?Content-Type=text/html
```

Just enter the terminal stream URL on the page that comes from the output of the script. It is similar to the one below.

```
https://www.theme25.com/b258f2e24b31bdb629f48ce28cc8aa59b2ab4aa3bc14c21e1840801493c65359.tig?Content-Type=image/png
```

This is the example web page that opens a stream right away. Stream urls usually point to two internal urls, one receiving the media, the other one forwarding the clicks and typing on the webpage.

```
open https://www.theme25.com/345b6a50c6bc0ae0c120f134f7a98003bcc8e411cdccdc7ff52c9586bd2a29d0.tig?Content-Type=text/html#https://www.theme25.com/49a7ca504add5d00a8f66dd4f3ddc62ef6e61d4ebb733f0fbbf6c5e50dcacf13.tig?Content-Type=text/plain
```

The example script_to_png.c actually uses typer.c inside.

Use the unit tests as a quality assurance of each encoding primitive retaining basic featuress.

```
cat test.c | grep gcc | bash
```

A more sophisticated example are the pair of sender.c and receiver.c. They simulate lossy UDP traffic over the network running the encoder and decoder in separate processe. TODO The codec needs some minor error correction fixes here.

```
cat receiver.c | grep gcc | bash
```

Run this in another terminal in WSL2.

```
cat sender.c | grep gcc | bash
```

TODO The samples need the audio logic added to the time stamp logic.