#compdef _bbc bbc

_list_blocks() {
    ids=()
    bbc list | while read id execpath; do
        if [[ "$1" == "eachmon" ]]; then
            xrandr | grep " connected" | awk '{print $1}' | while read disp; do
                ids+=$(echo "$id\\:$disp:$execpath")
            done
        fi
        ids+="$id:$execpath"
    done
    _describe -V 'block ids' ids
}

_list_properties() {
    props=()
    bbc list-properties | while read t n d; do
        props+="$n:$d"
    done
    _describe 'properties' props
}

_list_settings() {
    settings=()
    module=
    bbc list-settings | while read t n d; do
        [[ -z "$t" ]] && continue

        if [[ "$t" =~ :$ ]]; then
            module="${t:0:-1}\\:"
            continue
        fi

        settings+="$module$n:$d"
    done
    _describe -V 'settings' settings
}

_list_modules() {
    _values 'modules' $(bbc list-modules | awk '{print $1}')
}

_complete_property_value() {
    property=$words[$(( $CURRENT - 1 ))]
    t=$(bbc list-properties | awk -v p=$property '$2==p {print $1}')

    case $property in
    module)
        _list_modules
        return
        ;;
    exec)
        _path_files -P "/" -W "/"
        return
        ;;
    esac

    case $t in
    bool)
        _values 'boolean' 'true' 'false'
        return
        ;;
    position)
        _values 'position' 'left' 'center' 'right'
        return
        ;;
    esac
}

_complete_setting_value() {
    setting=$words[$(( $CURRENT - 1 ))]

    case $setting in
    position)
        _values 'position' 'top' 'bottom'
        return
        ;;
    trayside)
        _values 'position' 'left' 'right'
        return
        ;;
    esac

    module=
    vt=

    bbc list-settings | while read t n d; do
        [[ -z "$t" ]] && continue

        if [[ "$t" =~ :$ ]]; then
            module="${t:0:-1}:"
            continue
        fi

        if [[ "$setting" == "$module$n" ]]; then
            vt=$t
            break
        fi
    done

    case $vt in
    bool)
        _values 'boolean' 'true' 'false'
        return
        ;;
    position)
        _values 'position' 'left' 'center' 'right'
        return
        ;;
    esac
}

_comp_exec() {
    case $CURRENT in
    3)
        _list_blocks
        ;;
    esac
}

_comp_property() {
    case $CURRENT in
    3)
        _list_blocks eachmon
        ;;
    4)
        _list_properties
        ;;
    5)
        _complete_property_value
        ;;
    esac
}

_comp_setting() {
    case $CURRENT in
    3)
        _list_settings
        ;;
    4)
        _complete_setting_value
        ;;
    esac
}

_comp_new() {
    case $CURRENT in
    3)
        _values 'options' '--eachmon'
        ;;
    esac
}

_comp_rm() {
    case $CURRENT in
    3)
        _list_blocks
        ;;
    esac
}

_comp_move-left() {
    case $CURRENT in
    3)
        _list_blocks
        ;;
    esac
}

_comp_move-right() {
    case $CURRENT in
    3)
        _list_blocks
        ;;
    esac
}

_comp_dump() {
    case $CURRENT in
    3)
        _values 'options' '--explicit'
        ;;
    esac
}

_comp_load-module() {
    case $CURRENT in
    3)
        _path_files -P "/" -W "/"
        ;;
    esac
}

_comp_unload-module() {
    case $CURRENT in
    3)
        _list_modules
        ;;
    esac
}

_exists() {
    declare -f -F $1 > /dev/null
    return $?
}

_bbc()
{
    if [[ "$CURRENT" == "2" ]]; then
        cmds=()
        bbc --help | tail -n+4 | while read line; do
            cmds+=$(echo "$line" | sed "s/^\s*//g" | sed -E "s/\s.*\s{2,}/:/g")
        done
        _describe -t commands 'bbc' cmds
    else
        _exists _comp_$words[2] && _comp_$words[2]
    fi
}
