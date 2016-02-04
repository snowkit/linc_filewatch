
import filewatch.Filewatch;

class Test {

    static function main() {

        Filewatch.init(function(event:FilewatchEvent) {
            trace('type: ${event.type} path: ${event.path}');
        });

        Filewatch.add_watch(Sys.getCwd());

        try {
            sys.FileSystem.deleteFile('./test.file1');
            sys.FileSystem.deleteFile('./test.file2');
            sys.FileSystem.deleteFile('./test.file3');
            sys.FileSystem.deleteFile('./test.file4');
        } catch(e:Dynamic) {}

        trace('inited?');

        var count = 0;

        while(count < 4) {
            count++;
            trace('count: $count');
            sys.io.File.saveContent('./test.file$count', 'linc filewatch example ' + Math.random());
            Sys.sleep(1);
        }

        Filewatch.shutdown();

        trace('exit');

    }

}