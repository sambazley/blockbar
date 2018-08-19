function __fish_bbc
    set cmd (commandline -opc)
    if [ (count $cmd) -eq 1 ]
        bbc --help | tail -n+4 | sed 's/^\s*//g' | sed -E 's/\s.*\s{2,}/\t/g'
        return
    else if [ (count $cmd) -eq 2 ]
        if [ $cmd[2] = 'exec' -o $cmd[2] = 'property' -o $cmd[2] = 'rm' \
             -o $cmd[2] = 'move-left' -o $cmd[2] = 'move-right' ]
            bbc list
        else if [ $cmd[2] = 'setting' ]
            bbc list-settings | sed 's/^[a-zA-Z]*\s*//g' | sed 's/\s\{2,\}/\t/g'
        end
    else if [ (count $cmd) -eq 3 ]
        if [ $cmd[2] = 'property' ]
            bbc list-properties | sed 's/^[a-zA-Z]*\s*//g' | sed 's/\s\{2,\}/\t/g'
        else if [ $cmd[2] = 'setting' ]
            if [ $cmd[3] = 'position' ]
                echo 'top'
                echo 'bottom'
            else if [ $cmd[3] = 'traybar' ]
                xrandr | grep '\sconnected' | awk '{print $1}'
            else if [ $cmd[3] = 'trayside' ]
                echo 'left'
                echo 'right'
            end
        else if [ $cmd[2] = 'new' ]
            echo '--eachmon'
        else if [ $cmd[2] = 'dump' ]
            echo '--explicit'
        end
    else if [ (count $cmd) -eq 4 ]
        if [ $cmd[2] = 'property' ]
            if [ $cmd[4] = 'pos' ]
                echo 'left'
                echo 'center'
                echo 'right'
            end
        end
    end
end

complete -c bbc -xa '(__fish_bbc)'
