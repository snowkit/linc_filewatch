package filewatch;

import haxe.io.Path;

enum abstract FilewatchEventType(Int) from Int to Int {
    var FWE_unknown     = 0;
    var FWE_modify      = 1;
    var FWE_remove      = 2;
    var FWE_create      = 3;
    var FWE_renamed_old = 4;
    var FWE_renamed_new = 5;

    inline public function toString() {
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
@:allow(filewatch.FilewatchInternal)
#if !display
@:build(linc.Linc.touch())
@:build(linc.Linc.xml('filewatch'))
#end
extern class Filewatch {
    @:native('linc::filewatch::add_watch')
    private static function _add_watch(_path:String):Void;

    @:native('linc::filewatch::remove_watch')
    private static function _remove_watch(_path:String) : Bool;

    inline public static function add_watch(_path:String, _callback:FilewatchEvent->Void)
        FilewatchInternal.add_watch(_path, _callback);

    inline public static function remove_watch(_path:String)
        FilewatchInternal.remove_watch(_path);

    @:native('linc::filewatch::update')
    static function update() : Void;

    @:native('linc::filewatch::shutdown')
    static function shutdown() : Void;

    @:native('linc::filewatch::init')
    private static function _internal_init(func:cpp.Callable<Int->String->String->Void>): Bool;

} //Filewatch

@:allow(filewatch.Filewatch)
private class FilewatchInternal {

    static var callbacks = new Map<String, FilewatchEvent->Void>();
    static var inited : Bool = false;

    static function add_watch(_path:String, _callback:FilewatchEvent->Void):Void {
        if(inited == false)
            inited = Filewatch._internal_init(cpp.Callable.fromStaticFunction(internal_callback));

        // make sure the path is absolute!
        if (!Path.isAbsolute(_path))
            _path = Path.join([Path.removeTrailingSlashes(Path.normalize(Sys.getCwd())), _path]);

        callbacks.set(_path, _callback);
        Filewatch._add_watch(_path);
    }

    static function remove_watch(_path:String):Void {
        Filewatch._remove_watch(_path);
        callbacks.remove(_path);
    }

    static function internal_callback(_type:Int, _path:String, _rel_path:String) : Void {
        final evt:FilewatchEvent = {
            timestamp: haxe.Timer.stamp(),
            type: _type,
            watch_path: _path,
            rel_file_path: _rel_path
        };

        var shouldSkip = !inited || !callbacks.exists(_path);
        #if LINC_FILEWATCH_VERBOSE
            trace(evt.type.toString() + ' - ' + _path + ' - ' + _rel_path + " (skipped: "+ shouldSkip +")");
        #end
        if(shouldSkip) return;

        final callback = callbacks.get(_path);
        callback(evt);
    }

}