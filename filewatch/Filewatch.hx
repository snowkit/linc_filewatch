package filewatch;

enum abstract FilewatchEventType(Int) from Int to Int {
    var FWE_unknown     = 0;
    var FWE_modify      = 1;
    var FWE_remove      = 2;
    var FWE_create      = 3;
    var FWE_renamed_old = 4;
    var FWE_renamed_new = 5;

    inline function toString() {
        return switch(this) {
            case FWE_unknown:   'FWE_unknown';
            case FWE_modify:    'FWE_modify';
            case FWE_remove:    'FWE_remove';
            case FWE_create:    'FWE_create';
            case FWE_renamed_old: 'FWE_renamed_old';
            case FWE_renamed_new: 'FWE_renamed_new';
            case _:             '$this';
        }
    }
}

@:structInit
class FilewatchEvent {
    public var timestamp:Float;
    public var type: FilewatchEventType;
    public var watch_path: String;      // path we are watching
    public var rel_file_path: String;   // relative path to file
}

@:keep
@:include('linc_filewatch.h')
#if !display
@:build(linc.Linc.touch())
@:build(linc.Linc.xml('filewatch'))
#end
extern class Filewatch {

    @:allow(filewatch.FilewatchInternal)
    @:native('linc::filewatch::add_watch')
    private static function _add_watch(_path:String):Void;

    @:allow(filewatch.FilewatchInternal)
    @:native('linc::filewatch::remove_watch')
    private static function _remove_watch(_path:String):Bool;

    @:allow(filewatch.FilewatchInternal)
    @:native('linc::filewatch::init')
    private static function internal_init(func:cpp.Callable<Int->String->String->Void>):Void;

    @:allow(filewatch.FilewatchInternal)
    @:native('linc::filewatch::shutdown')
    private static function _shutdown():Void;

    inline public static function add_watch(_path:String, _cb:FilewatchEvent->Void) {
        FilewatchInternal.add_watch(_path, _cb);
    }

    inline public static function remove_watch(_path:String) {
        FilewatchInternal.remove_watch(_path);
    }

    inline public static function shutdown():Void {
        FilewatchInternal.shutdown();
    }

    inline public static function update():Void {
        while(true) {
            var evt = FilewatchInternal.deque.pop(false);
            if (evt == null) break;
            FilewatchInternal.callbacks.get(evt.watch_path)(evt);
        }
    }
}

@:allow(filewatch.Filewatch)
private class FilewatchInternal {

    static var inited = false;
    static var callbacks = new Map<String, FilewatchEvent->Void>();
    static var deque = new sys.thread.Deque<FilewatchEvent>();
    static var evtbuffer = new Map<Int, Float>();

    static function add_watch(_path:String, _callback:FilewatchEvent->Void):Void {
        if(inited == false) {
            inited = true;
            Filewatch.internal_init( cpp.Callable.fromStaticFunction(internal_callback) );
        }
        callbacks.set(_path, _callback);
        Filewatch._add_watch(_path);
    }
    
    static function remove_watch(_path:String):Void {
        Filewatch._remove_watch(_path);
        callbacks.remove(_path);
    }

    static function shutdown():Void {
        Filewatch._shutdown();
        callbacks.clear();
    }

    static function internal_callback(_type:FilewatchEventType, _watch_path:String, _rel_path:String):Void {
        var callback = callbacks.get(_watch_path);
        if(!inited || callback == null) return;

        var event:FilewatchEvent = {
            timestamp: haxe.Timer.stamp(),
            type: _type,
            watch_path: _watch_path,
            rel_file_path: _rel_path
        };

        var hash = djb2('$_type$_watch_path$_rel_path');
        if (evtbuffer.exists(hash)) {
            // make sure that the very same event cannot fire too quickly. 
            // This is a guard against the doubled modified events on windows and 
            // shouldnt hurt us on other platforms
            if (event.timestamp - evtbuffer.get(hash) > 0.1) {
                deque.add(event);
                evtbuffer.set(hash, event.timestamp);
            }
        } else {
            deque.add(event);
            evtbuffer.set(hash, event.timestamp);
        }        
    }

    //djb2 http://www.cse.yorku.ca/~oz/hash.html
    static function djb2(str:String):Int {    
        var hash:Int = 5381;
        for (i in 0...str.length) {
            hash = ((hash << 5) + hash) + str.charCodeAt(i);
            hash = hash & hash; // 32 bit
        }
        return hash;
    }

}