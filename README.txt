What is this?
-------------
It's a cli ft8 decoder for multiple bands.
Right now it's still very early in development. If you know C/C++, feel free
to jump in!

Will it transmit?
-----------------
Our grand ambition is to do so, yes. But for now, we'll settle for decoding
them all and displaying alerts.

How to use it?
--------------
Copy config.json.exmaple to config.json and edit it...
Ex:
	cp config.json{.example,}
	joe config.json

Start the mess:
	./startup

In theory, the client will keep the ft8 decoders/etc running, check the logs
(ft8decoder.$band.log)


If you install (make install), you can avoid the ./startup script and call ft8goblin directly.
It sets up library pathes, etc so that libraries in lib/ will be used by the programs.

For now installing isn't fully supported, please report any bugs you find with it!
