# CODEC21

The project is an audio visual codec implementation. Codecs encode and decode media into digital formats usually focusing on a single goal like bandwidth, size, latency, power use, processor use, or special network topologies.

This implementation is focusing on flexibility. It allows AI models to refine the codebase enabling low power use, a rare feature among codecs.

We are also very simple making the lowest cost devices possible. Manufacturers can get the bang for the buck skewing the BOM to better and bigger display panels over industrial PCs behind them increasing customer value. Defense companies can focus on lower cost working assets rather than a long term investment making the BOM cheaper allowing bigger ranges or heavier payloads. Education can leverage the codebase with our flexible patent licensing requiring payments only on EBITDA.

First you need to download and prepare some test files. We use PNGs read from the disk directly reducing the decoding overhead of H.264, H.265. This allows more accurate benchmarking of our own codebase.

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

Render and stream the console to a webpage using a series of PNG files.

```
cat script_to_png.c | grep gcc | bash
```

Open the html in the browser like below.

```
https://theme25.com/826c3607194640b63f9beb42a6ed182e76700b1c8d120049a588f6ffc87ececd.tig?Content-Type=text/html
```

Just enter the terminal stream URL on the page that comes from the output of the script. It is similar to the one below.

```
https://www.theme25.com/b258f2e24b31bdb629f48ce28cc8aa59b2ab4aa3bc14c21e1840801493c65359.tig?Content-Type=image/png
```