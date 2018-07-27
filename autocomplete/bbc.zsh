#compdef _bbc bbc

_bbc()
{
    if [[ "$CURRENT" == "2" ]]; then
        cmds=()
        bbc --help | tail -n+4 | while read line; do
            cmds+=$(echo "$line" | sed "s/^\s*//g" | sed -E "s/\s.*\s{2,}/:/g")
        done
        _describe -t commands 'bbc' cmds
    elif [[ "$CURRENT" == "3" && "${words[2]}" =~ "(exec)|(property)|\
            (rm)|(move-out)|(move-in)" ]]; then
        ids=()
        bbc list | while read id; do
            ids+=$(echo $id | sed -E "s/\t/:/g")
        done
        _describe -t commands 'bbc' ids
    elif [[ "$CURRENT" == "4" && "${words[2]}" =~ "(property)" ]]; then
        props=()
        bbc list-props | while read prop; do
            props+=$(echo $prop | sed 's/^[a-zA-Z]*\s*//g' | sed 's/\s\{2,\}/:/g')
        done
        _describe -t commands 'bbc' props
    fi
}
