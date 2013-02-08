# Fast FIX (Financial Information Exchange) protocol parser [FFP]


Fast FIX Parser (FFP) is a library which converts chunks of data (raw bytes) 
as they arrive from e.g. a socket, into a representation of FIX messages that 
can be further analysed for semantic checks, converted into “business” structures, etc. 
It also provides a way to specify what tags are allowed for a particular message and 
verifies this specification at run-time.

### Why?

* Speed. On my rather old Core i5-430M 2.26GHz laptop, in a single thread, this parser can process about 260,000 messages with groups per second and about 520,000 simple messages per second. The processing time is more or less a linear function of the message length.
* It does not impose any particular I/O or threading model. In fact, it does no I/O at all, and there are no threads running in the background. This greatly simplifies integration of the library into an existing code base. 
* The parser does not expect every chunk of its input data to be a complete FIX message. The input bytes can be fed into the parser as they become available, and the parser splits or combines the input into complete messages.



###### For more details see doc/brief.html

###### Licence: BSD
