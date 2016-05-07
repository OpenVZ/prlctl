get_vms() {
	# TODO if remote parameters are specified -
	# use them here
	prlctl list -a -o name | awk '{if (NR > 1) print $1}'
}

get_uuids() {
	prlctl list -a -o uuid | awk '{if (NR > 1) print substr($1,2,36)}'
}

get_vm_ids() {
	printf "$(get_vms)\n$(get_uuids)\n"
}

get_snaps_names() {
	# TODO Don't know how to find snaps names easily
	true
}

get_snaps_ids() {
	# TODO Add checking of vm existence
	# TODO Don't work because of '{' :-(
	prlctl snapshot-list $1 | awk '{if (NR > 1) print substr($0, 41, 38)}'
}

get_backups_ids() {
	# TODO Add checking of vm existence
	# TODO Don't work because of '{' :-(
	prlctl backup-list $1 | awk '{if (NR > 1) print substr($0, 41, 38)}'
}

get_ostypes() {
	# TODO How to get supported OS types?
	true
}

get_distros() {
	# TODO How to get supported distros?
	# TODO: "prlctl create ads --distribution dsa" shows available distros
	true
}

get_ostemplates() {
	# available templates
	prlctl list -t -o name | awk '{if (NR > 1) print $1}'
}

get_devs() {
	# TODO Add checking of vm existence and device type/mode
	vmid=$1
	devtype=$2
	prlctl list -i $vmid | egrep '^[[:space:]]+[[:alnum:]]+ \([+-]\)' | awk '{print $1}'
}

_prlctl()
{
	local cur prev opts base
	COMPREPELY=()
	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD-1]}"

	local actions_on_vmid="capture clone delete enter migrate \
		pause reset resume start stop snapshot snapshot-delete \
		snapshot-list snapshot-switch suspend statistics unregister \
		set backup restore backup-list backup-delete reset-uptime \
		move exec"
	local actions_without_vmid="create list register server"

	local capture_flags='--file'
	local clone_flags='--name'
	local clone_optional_flags='--template --location'
	local create_flags='-c --config --location -o --ostype -d --distribution --ostemplate'
	local list_flags='-a --all -t --template -o --output -s --sort -i --info'
	local migrate_flags='--location --mode --no-compression'
	local snapshot_flags='-n --name -d --description'
	local snapshotlist_flags='-t --tree -i --id'
	local snapshotdelete_flags='-i --id'
	local snapshotswitch_flags='-i --id'
	local backup_flags='--description -f --full -i --incremental --no-compression'
	local backuplist_flags='-f --full --localvms'
	local backupdelete_flags='-t --tag'
	local statistics_flags='--loop --filter'
	local set_flags='--cpus --memsize --videosize --description \
		--onboot --name --device-add --device-del --device-set \
		--device-connect --device-disconnect --applyconfig \
		--netfilter --swappages --swap --quotaugidlimit \
		--autostart --autstart-delay --autostop --start-as-user \
		--vnc-mode --vnc-port --vnc-passwd --shf-host --shf-host-add \ 
		--shf-host-set --shf-host-del --shf-guest --shf-guest-automount --ioprio \
		--ha-enable --ha-prio'
	local set_device_hdd_flags='--image --type --size --split --iface --position --device --passthr'
	local set_device_cdrom_flags='--device --image --iface --position --passthr'
	local set_device_net_flags='--type --mac --iface'
	local set_device_fdd_flags='--device --image --recreate'
	local set_device_serial_flags='--device --output --socket'
	local move_flags='--location'

	local global_flags='-l -p -v --verbose'

	if [ $COMP_CWORD == 1 ]; then
		opts="${actions_on_vmid} ${actions_without_vmid}"
	else
		for i in $actions_on_vmid; do
			if [ "${prev}" = "${i}" ]; then
				if [ -z "${cur}" ]; then
					opts=$(get_vms)
				else
					opts=$(get_vm_ids)
				fi
				break
			fi
		done

	fi

	if [ -z "${opts}" ]; then

		# processing all options that require arguments
		case "${prev}" in
		-n|--name)
			if [ "${COMP_WORDS[1]}" = 'clone' ]; then
				opts=$(get_vms)
			else
				opts=$(get_snaps_names)
			fi
			;;
		--location)
			COMPREPLY=($(compgen -o dirnames -- "${cur}"))
			return 0
			;;
		-c|--config|--file)
			# TODO Add filtering on file extentions
			COMPREPLY=($(compgen -A file -- "${cur}"))
			return 0
			;;
		--ostype)
			opts=$(get_ostypes)
			;;
		--distribution)
			opts=$(get_distros)
			;;
		--ostemplate)
			opts=$(get_ostemplates)
			;;
		--output)
			opts=$(get_vm_ids)
			;;
		-s|--sort)
			opts='name -name'
			;;
		--onboot)
			opts='yes no auto'
			;;
		--device-add)
			opts='hdd cdrom net fdd serial usb'
			;;
		--device-del)
			opts=$(get_devs "${COMP_WORDS[2]}")
			;;
		--device-connect)
			opts=$(get_devs "${COMP_WORDS[2]}" 'disconnected')
			;;
		--device-disconnect)
			opts=$(get_devs "${COMP_WORDS[2]}" 'connected')
			;;
		--device-set)
			opts=$(get_devs "${COMP_WORDS[2]}")
			;;
		--shf-host|--shf-guest|--shf-guest-automount)
			opts='on off'
			;;
		--ioprio)
			# ioprio range: 0 - 7 (default : 4)
			opts='0 1 2 3 4 5 6 7'
			;;
		--cpus)
			# max vcpus
			local cores=$(grep '^cpu cores' /proc/cpuinfo | wc -l )
			local proc=$(grep 'processor' /proc/cpuinfo | wc -l)
			opts=$[ $cores * $proc ]
			;;
		--memsize)
			# max available memory
			opts=$(cat /proc/meminfo | grep  "MemTotal:" | awk '{print $2}')
			;;
		--autostart)
			opts="on off auto"
			;;
		--autostop)
			opts="stop suspend"
			;;
		--vnc-mode)
			opts="auto manual off"
			;;
		--ha-enable)
			opts='yes no'
			;;
		--mode)
			if [ "${COMP_WORDS[1]}" = 'migrate' ]; then
				opts='cold warm hot'
			else
				opts='ro rw'
			fi
			;;
		--path)
			COMPREPLY=($(compgen -o dirnames -- "${cur}"))
			return 0
			;;
		--type)
			# TODO for hdd: ide|scsi, for net:host|shared|bridget
			opts=''
			;;
		--id)
			opts="$(get_snaps_ids "${COMP_WORDS[2]}")"
			# TODO Fucking '{'
			[ -z "${cur}" ] && cur='{'
			;;
		-o)
			if [ "${COMP_WORDS[1]}" = 'create' ]; then
				opts=$(get_ostypes)
			else
				opts=$(get_vm_ids)
			fi
			;;
		-i)
			action="${COMP_WORDS[1]}"
			if [ "${action::8}" = 'snapshot' ]; then
				# TODO See '--id'
				# opts=$(get_snaps_ids)
				opts=""
			else
				opts=$(get_ostypes)
			fi
			;;
		--autostart)
			opts='stop suspend'
			;;
		--autostop)
			opts='on off auto'
			;;
		--start-as-user)
			opts='administrator owner'
			;;
		--applyconfig)
			COMPREPLY=($(compgen -A file -- "${cur}"))
			return 0
			;;
		--netfilter)
			opts='disabled stateless stateful full'
			;;
		--filter)
			# TODO find out names for statistics filering
			opts=''
			;;
		-l)
			# user:passwd@server
			opts=''
			;;
		-p|--read-passwd)
			COMPREPLY=($(compgen -A file -- "${cur}"))
			return 0
			;;
		-v)
			# TODO throw out -v
			opts=$global_flags
			;;
		--verbose)
			# TODO find out acceptable log levels
			opts='1 2 3'
			;;

		*) # processing actions' local options

			case "${COMP_WORDS[1]}" in
			backup)
				opts="${backup_flags} ${global_flags}" 
				;;
			backup-delete)
				if [ $COMP_CWORD == 3 ]; then
					opts="${backupdelete_flags}" 
				else
					opts="${global_flags}"
				fi
				;;
			backup-list)
				opts="${backuplist_flags} ${global_flags}" 
				;;
			capture)
				[ $COMP_CWORD == 3 ] && opts="${capture_flags}"
				;;
			clone)
				case "${COMP_CWORD}" in
				3)
					opts="${clone_flags}"
					;;
				*)
					opts="${clone_optional_flags} ${global_flags}"
					;;
				esac
				;;
			create)
				# TODO Add exceptions for different parameter
				# configurations
				[ $COMP_CWORD == 3 ] && opts="${create_flags} ${global_flags}"
				;;
			delete)
				opts="${global_flags}"
				;;
			enter)
				opts="${global_flags}"
				;;
			list)
				opts="${list_flags} ${global_flags}"
				;;
			migrate)
				opts="${migrate_flags} ${global_flags}"
				;;
			move)
				case "${COMP_CWORD}" in
				3)
					opts="${move_flags}"
					;;
				*)
					opts="${global_flags}"
					;;
				esac
				;;
			pause)
				opts="${global_flags}"
				;;
			register)
				if [ $COMP_CWORD == 2 ]; then
					COMPREPLY=($(compgen -A file -- "${cur}"))
					return 0
				else
					opts="$global_flags"
				fi
				;;
			reset)
				opts="${global_flags}"
				;;
			reset-uptime)
				opts="${global_flags}"
				;;
			resume)
				opts="${global_flags}"
				;;
			server)
				if [ $COMP_CWORD == 2 ]; then
					opts='shutdown info'
				else
					opts="${global_flags}"
				fi
				;;
			start)
				opts="${global_flags}"
				;;
			stop)
				opts="${global_flags}"
				;;
			snapshot)
				opts="${snapshot_flags} ${global_flags}" 
				;;
			snapshot-delete)
				if [ $COMP_CWORD == 3 ]; then
					opts="${snapshotdelete_flags}" 
				else
					opts="${global_flags}"
				fi
				;;
			snapshot-list)
				opts="${snapshotlist_flags} ${global_flags}" 
				;;
			snapshot-switch)
				if [ $COMP_CWORD == 3 ]; then
					opts="${snapshotswitch_flags}" 
				else
					opts="${global_flags}"
				fi
				;;
			suspend)
				opts="${global_flags}"
				;;
			statistics)
				opts="${statistics_flags} ${global_flags}"
				;;
			unregister)
				opts="${global_flags}"
				;;
			set)
				opts="${set_flags} ${global_flags}"
				;;
			*)
				;;
			esac
			;;
		esac
	fi

	COMPREPLY=($(compgen -W "${opts}" -- "${cur}"))

	return 0
}

complete -F _prlctl prlctl
