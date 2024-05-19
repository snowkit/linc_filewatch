
import filewatch.Filewatch;

class Test {

    static var start_time = 0.0;
    static var looping = true;
    static var step = 0;
    static var file = 'test.file';

    static function main() {

        start_time = Date.now().getTime();

        trace('start ' + start_time);

        var cwd = Sys.getCwd();
        file = cwd + file;

        trace('cwd is : $cwd');
        trace('file should be : $file');

        Filewatch.add_watch(cwd, (event:FilewatchEvent)->{
            trace('type: ${event.type} path: ${event.rel_file_path}');
        });

        while(looping) loop();
        
        Filewatch.shutdown();

        trace('done');

    } //main


    static function loop() {

        Filewatch.update();

        var time_since_start : Float = (Date.now().getTime() - start_time)/1000;

        if(time_since_start > 4) {
            looping = false;
            return;
        }

        if(time_since_start == 1 && step != 1) {
            step++;
            trace('step 1 - create');
            sys.io.File.saveContent(file, 'linc filewatch ' + Math.random());
        }

        if(time_since_start == 2 && step != 2) {
            step++;
            trace('step 2 - modify');
            sys.io.File.saveContent(file, 'linc filewatch ' + Math.random());
        }

        if(time_since_start == 3 && step != 3) {
            step++;
            trace('step 3 - remove');
            sys.FileSystem.deleteFile(file);
        }

    }

}