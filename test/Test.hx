
import filewatch.Filewatch;

    #if (!mac && !linux && !windows)
        #error "You should define a target, please read build.hxml"
    #end

class Test {

    static function main() {

        trace('start');

        Filewatch.init(function(event:FilewatchEvent) {
            trace('type: ${event.type} path: ${event.path}');
        });

        Filewatch.add_watch(Sys.getCwd());        

        var idx = 0;
        while(idx < 4) {
            idx++;
            sys.io.File.saveContent('./test.file$idx', 'linc filewatch ' + Math.random());
            Sys.sleep(1);
            sys.FileSystem.deleteFile('./test.file$idx');
            Sys.sleep(1);
        }

        Filewatch.shutdown();

        trace('exit');

    }

}