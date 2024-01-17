die() # $1 = exit code, use : to not exit when printing warnings $@ = exit or warning messages
{
	ret="$1"
	shift 1
	printf %s\\n "$@" >&2
	case "$ret" in
		: ) return 0 ;;
		* ) exit "$ret" ;;
	esac
}

print_help_option() # $1 = option $@ = description
{
	_opt="$1"
	shift 1
	printf '  %-26s  %s\n' "$_opt" "$@"
}

print_help()
{	cat << EOF
====================
 Quickbuild script
====================
Package: $PACKAGE_NAME

General environment variables:
  CC:         C compiler
  CFLAGS:     C compiler flags
  CXX:        C++ compiler
  CXXFLAGS:   C++ compiler flags
  LDFLAGS:    Linker flags

General options:
EOF
	print_help_option "--prefix=PATH"            "Install path prefix"
	print_help_option "--sysconfdir=PATH"        "System wide config file prefix"
	print_help_option "--bindir=PATH"            "Binary install directory"
	print_help_option "--datarootdir=PATH"       "Read-only data install directory"
	print_help_option "--docdir=PATH"            "Documentation install directory"
	print_help_option "--mandir=PATH"            "Manpage install directory"
	print_help_option "--global-config-dir=PATH" "System wide config file prefix (Deprecated)"
	print_help_option "--build=BUILD"            "The build system (no-op)"
	print_help_option "--host=HOST"              "Cross-compile with HOST-gcc instead of gcc"
	print_help_option "--help"                   "Show this help"

	echo ""
	echo "Custom options:"

	while read -r VAR COMMENT; do
		TMPVAR="${VAR%=*}"
		COMMENT="${COMMENT#*#}"
		VAL="${VAR#*=}"
		VAR="$(echo "${TMPVAR#HAVE_}" | tr '[:upper:]' '[:lower:]')"
		case "$VAR" in
			'c89_'*) continue;;
			*)
			case "$VAL" in
				'yes'*)
					print_help_option "--disable-$VAR" "Disable $COMMENT";;
				'no'*)
					print_help_option "--enable-$VAR" "Enable  $COMMENT";;
				'auto'*)
					print_help_option "--enable-$VAR" "Enable  $COMMENT"
					print_help_option "--disable-$VAR" "Disable $COMMENT";;
				*)
					print_help_option "--with-$VAR" "Config  $COMMENT";;
			esac
		esac
	done < 'qb/config.params.sh'
}

opt_exists() # $opt is returned if exists in OPTS
{	opt="$(echo "$1" | tr '[:lower:]' '[:upper:]')"
	err="$2"
	eval "set -- $OPTS"
	for OPT do [ "$opt" = "$OPT" ] && return; done
	die 1 "Unknown option $err"
}

parse_input() # Parse stuff :V
{	OPTS=; while read -r VAR _; do
		TMPVAR="${VAR%=*}"
		OPTS="$OPTS ${TMPVAR##HAVE_}"
	done < 'qb/config.params.sh'
	#OPTS contains all available options in config.params.sh - used to speedup
	#things in opt_exists()
	
	while [ "$1" ]; do
		case "$1" in
			--prefix=*) PREFIX=${1##--prefix=};;
			--global-config-dir=*|--sysconfdir=*) GLOBAL_CONFIG_DIR="${1#*=}";;
			--bindir=*) BIN_DIR="${1#*=}";;
			--build=*) BUILD="${1#*=}";;
			--datarootdir=*) SHARE_DIR="${1#*=}";;
			--docdir=*) DOC_DIR="${1#*=}";;
			--host=*) CROSS_COMPILE=${1##--host=}-;;
			--mandir=*) MAN_DIR="${1#*=}";;
			--enable-*)
				opt_exists "${1##--enable-}" "$1"
				eval "HAVE_$opt=yes"
			;;
			--disable-*)
				opt_exists "${1##--disable-}" "$1"
				eval "HAVE_$opt=no"
				eval "HAVE_NO_$opt=yes"
			;;
			--with-*)
				arg="${1##--with-}"
				val="${arg##*=}"
				opt_exists "${arg%%=*}" "$1"
				eval "$opt=\"$val\""
			;;
			-h|--help) print_help; exit 0;;
			*) die 1 "Unknown option $1";;
		esac
		shift
	done
}

. qb/config.params.sh

parse_input "$@"
