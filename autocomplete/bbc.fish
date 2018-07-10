function __fish_bbc
    set cmd (commandline -opc)
    if [ (count $cmd) -eq 1 ]
        bbc --help | tail -n+4 | sed 's/^\s*//g' | sed -E 's/\s.*\s{2,}/\t/g'
        return
    else if [ (count $cmd) -eq 2 -a $cmd[2] = 'exec' ]
        bbc list
    end
end

complete -c bbc -xa '(__fish_bbc)'
