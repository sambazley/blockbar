function __fish_bbc
    set cmd (commandline -opc)
    if [ (count $cmd) -eq 1 ]
        bbc --help | tail -n+4 | sed 's/^\s*//g' | sed -E 's/\s.*\s{2,}/\t/g'
        return
    else if [ (count $cmd) -eq 2 ]
        if [ $cmd[2] = 'exec' -o $cmd[2] = 'property' -o $cmd[2] = 'rm' \
             -o $cmd[2] = 'move-out' -o $cmd[2] = 'move-in' ]
            bbc list
        else if [ $cmd[2] = 'setting' ]
            bbc list-settings | sed 's/^[a-zA-Z]*\s*//g' | sed 's/\s\{2,\}/\t/g'
        end
    else if [ (count $cmd) -eq 3 ]
        if [ $cmd[2] = 'property' ]
            bbc list-properties | sed 's/^[a-zA-Z]*\s*//g' | sed 's/\s\{2,\}/\t/g'
        end
    end
end

complete -c bbc -xa '(__fish_bbc)'
