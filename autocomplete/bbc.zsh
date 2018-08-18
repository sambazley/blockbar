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
    elif [[ "$CURRENT" == "3" && "${words[2]}" == "setting" ]]; then
        settings=()
        bbc list-settings | while read s; do
            settings+=$(echo $s | sed 's/^[a-zA-Z]*\s*//g' | sed 's/\s\{2,\}/:/g')
        done
        _describe -t commands 'bbc' settings
    elif [[ "$CURRENT" == "3" && "${words[2]}" == "new" ]]; then
        list=("--eachmon")
        _describe -t commands 'bbc' list
    elif [[ "$CURRENT" == "3" && "${words[2]}" == "dump" ]]; then
        list=("--explicit")
        _describe -t commands 'bbc' list
    elif [[ "$CURRENT" == "4" && "${words[2]}" == "property" ]]; then
        props=()
        bbc list-properties | while read prop; do
            props+=$(echo $prop | sed 's/^[a-zA-Z]*\s*//g' | sed 's/\s\{2,\}/:/g')
        done
        _describe -t commands 'bbc' props
    elif [[ "$CURRENT" == "4" && "${words[2]}" == "setting" ]]; then
        if [[ "${words[3]}" == "position" ]]; then
            list=("top" "bottom")
            _describe -t commands 'bbc' list
        elif [[ "${words[3]}" == "traybar" ]]; then
            list=($(xrandr | grep "\sconnected" | awk '{print $1}'))
            _describe -t commands 'bbc' list
        elif [[ "${words[3]}" == "trayside" ]]; then
            list=("left" "right")
            _describe -t commands 'bbc' list
        fi
    elif [[ "$CURRENT" == "5" && "${words[2]}" == "property" ]]; then
        if [[ "${words[4]}" == "pos" ]]; then
            list=("left" "center" "right")
            _describe -t commands 'bbc' list
        fi
    fi
}
