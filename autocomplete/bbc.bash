_bbc_append()
{
    COMPREPLY[${#COMPREPLY[@]}]=$1
}

_bbc_print_commands()
{
    for i in $(bbc --help | awk '{if (NR>3) {print $1}}'); do
        if [[ "$i" == $1* ]]; then
            _bbc_append "$i"
        fi
    done
}

_bbc()
{
    _init_completion || return

    argc=0
    argv=()
    for i in $COMP_LINE; do
        ((argc++))
        argv[${#argv[@]}]=$i
    done

    if [[ "$COMP_LINE" =~ [[:space:]]$ ]]; then
        ((argc++))
    fi

    if [[ "$argc" == "2" ]]; then
        case ${argv[1]} in
            -*)
                _bbc_append "--help"
                ;;
            *)
                _bbc_print_commands ${argv[1]}
                ;;
        esac
    fi
}

complete -F _bbc bbc
