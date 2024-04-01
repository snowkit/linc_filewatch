package filewatch;


enum abstract FilewatchEventType(Int) from Int to Int {
    var FWE_unknown     = 0;
    var FWE_modify      = 1;
    var FWE_remove      = 2;
    var FWE_create      = 3;

    inline function toString() {
        return switch(this) {
            case FWE_unknown:   'FWE_unknown';
            case FWE_modify:    'FWE_modify';
            case FWE_remove:    'FWE_remove';
            case FWE_create:    'FWE_create';
            case _:             '$this';
        }
    }
}

typedef FilewatchEvent = {
    var timestamp:Float;
    var type: FilewatchEventType;
    var path: String;       // full path
    var rel_path: String;   // relative to watched path
}

@:keep
@:include('linc_filewatch.h')
#if !display
@:build(linc.Linc.touch())
@:build(linc.Linc.xml('filewatch'))
#end
extern class Filewatch {

    static inline function init(_callback:FilewatchEvent->Void) : Bool return FilewatchInternal.init(_callback);

    @:native('linc::filewatch::add_watch')
    static function add_watch(_path:String) : Void;

    @:native('linc::filewatch::remove_watch')
    static function remove_watch(_path:String) : Bool;

    @:native('linc::filewatch::update')
    static function update() : Void;

    @:native('linc::filewatch::shutdown')
    static function shutdown() : Void;

    @:allow(filewatch.FilewatchInternal)
    @:native('linc::filewatch::init')
    private static function internal_init(func:cpp.Callable<Int->String->String->Void>): Bool;

} //Filewatch

private class FilewatchInternal {

    static var callback : FilewatchEvent->Void;
    static var inited : Bool = false;

    @:allow(filewatch.Filewatch)
    inline static function init(_callback:FilewatchEvent->Void) : Bool {

        callback = _callback;

        if(inited == false) {
            inited = true;
            return Filewatch.internal_init( cpp.Callable.fromStaticFunction(internal_callback) );
        }

        return inited;

    } //init

    static function internal_callback(_type:Int, _path:String, _rel_path:String) : Void {

        if(!inited || callback == null) return;

        callback({
            timestamp: haxe.Timer.stamp(),
            type: _type,
            path: _path,
            rel_path: _rel_path
        });

    } //internal_callback

}