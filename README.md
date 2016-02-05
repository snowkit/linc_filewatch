# linc/filewatch
Haxe/hxcpp @:native binding for OS level file watch notifications, for Mac,Windows and Linux.

This is a [linc](http://snowkit.github.io/linc/) library.

---

This library works with the Haxe cpp target only.

---

### Install

`haxelib git linc_filewatch https://github.com/snowkit/linc_filewatch.git`

### Example usage

See test/Test.hx

Note that the test may not emit events on some OS's because of the lack of a run loop on the CLI app. A more consistent way to test on your platform would be to include the library in your existing application and throw in the few lines needed to test it. The test therefore is more for reference, and not a working test case as of now.


