#!/bin/sh

set -eu

if [ "$#" -ne 2 ]; then
	echo "usage: $0 BUNDLE_PATH EXECUTABLE_PATH" >&2
	exit 2
fi

bundle_path=$1
executable_path=$2
library_paths=${COSMOE_BUNDLE_LIBRARY_PATHS:-/opt/homebrew/lib:/usr/local/lib}
extra_libraries=${COSMOE_BUNDLE_EXTRA_LIBRARIES:-}

case "$bundle_path" in
	'' | / | .)
		echo "Refusing unsafe bundle path: $bundle_path" >&2
		exit 2
		;;
esac

if [ ! -f "$executable_path" ]; then
	echo "Executable not found: $executable_path" >&2
	exit 2
fi

if ! command -v otool >/dev/null || ! command -v install_name_tool >/dev/null; then
	echo "macos-bundle.sh requires otool and install_name_tool" >&2
	exit 2
fi

contents_path="$bundle_path/Contents"
macos_path="$contents_path/MacOS"
frameworks_path="$contents_path/Frameworks"
main_executable="$macos_path/$(basename "$executable_path")"
work_path="$contents_path/.cosmoe-bundle-work"
todo_path="$work_path/todo"
current_path="$work_path/current"
manifest_path="$work_path/manifest"

is_apple_dependency()
{
	case "$1" in
		/usr/lib/* | /System/Library/* | /Library/Apple/* | \
		@rpath/libSystem.* | @rpath/libobjc.* | @rpath/libc++* | @rpath/libz.*)
			return 0
			;;
	esac
	return 1
}

canonical_path()
{
	directory=$(dirname "$1")
	filename=$(basename "$1")
	(cd "$directory" && printf '%s/%s\n' "$(pwd -P)" "$filename")
}

expand_rpath()
{
	path=$1
	loader_source=$2
	case "$path" in
		@loader_path/*)
			printf '%s/%s\n' "$(dirname "$loader_source")" "${path#@loader_path/}"
			;;
		@executable_path/*)
			printf '%s/%s\n' "$(dirname "$executable_path")" "${path#@executable_path/}"
			;;
		*)
			printf '%s\n' "$path"
			;;
	esac
}

resolve_dependency()
{
	dependency=$1
	loader_source=$2

	case "$dependency" in
		/*)
			[ -f "$dependency" ] && canonical_path "$dependency"
			return
			;;
		@loader_path/* | @executable_path/*)
			candidate=$(expand_rpath "$dependency" "$loader_source")
			[ -f "$candidate" ] && canonical_path "$candidate"
			return
			;;
		@rpath/*)
			dependency_name=${dependency#@rpath/}
			otool -l "$loader_source" | awk '
				$1 == "cmd" && $2 == "LC_RPATH" { in_rpath = 1; next }
				in_rpath && $1 == "path" { print $2; in_rpath = 0 }
			' | while IFS= read -r rpath; do
				candidate=$(expand_rpath "$rpath" "$loader_source")/$dependency_name
				if [ -f "$candidate" ]; then
					canonical_path "$candidate"
					exit 0
				fi
			done
			;;
	esac

	old_ifs=$IFS
	IFS=:
	for directory in $library_paths; do
		candidate=$directory/$(basename "$dependency")
		if [ -f "$candidate" ]; then
			IFS=$old_ifs
			canonical_path "$candidate"
			return
		fi
	done
	IFS=$old_ifs
}

has_rpath()
{
	otool -l "$1" | awk -v wanted="$2" '
		$1 == "cmd" && $2 == "LC_RPATH" { in_rpath = 1; next }
		in_rpath && $1 == "path" { if ($2 == wanted) found = 1; in_rpath = 0 }
		END { exit !found }
	'
}

add_rpath()
{
	if ! has_rpath "$1" "$2"; then
		install_name_tool -add_rpath "$2" "$1"
	fi
}

remove_external_rpaths()
{
	otool -l "$1" | awk '
		$1 == "cmd" && $2 == "LC_RPATH" { in_rpath = 1; next }
		in_rpath && $1 == "path" { print $2; in_rpath = 0 }
	' | while IFS= read -r rpath; do
		case "$rpath" in
			/opt/homebrew/* | /usr/local/*)
				install_name_tool -delete_rpath "$rpath" "$1"
				;;
		esac
	done
}

seen_source()
{
	awk -F '|' -v source="$1" '$1 == source { found = 1 } END { exit !found }' "$manifest_path"
}

seen_destination()
{
	awk -F '|' -v destination="$1" '$2 == destination { found = 1 } END { exit !found }' "$manifest_path"
}

rm -rf "$bundle_path"
mkdir -p "$macos_path" "$frameworks_path" "$work_path"
cp "$executable_path" "$main_executable"
: > "$manifest_path"
printf '%s|%s\n' "$(canonical_path "$executable_path")" "$main_executable" > "$todo_path"

for library_path in $extra_libraries; do
	if [ ! -f "$library_path" ]; then
		echo "Explicit runtime library not found: $library_path" >&2
		exit 1
	fi

	library_path=$(canonical_path "$library_path")
	framework_path="$frameworks_path/$(basename "$library_path")"
	if seen_destination "$framework_path"; then
		echo "Dylib name collision for $(basename "$library_path")" >&2
		exit 1
	fi
	cp -L "$library_path" "$framework_path"
	printf '%s|%s\n' "$library_path" "$framework_path" >> "$manifest_path"
	printf '%s|%s\n' "$library_path" "$framework_path" >> "$todo_path"
done

while [ -s "$todo_path" ]; do
	mv "$todo_path" "$current_path"
	: > "$todo_path"

	while IFS='|' read -r source_path bundled_path; do
		while IFS= read -r dependency; do
			[ -n "$dependency" ] || continue
			if is_apple_dependency "$dependency"; then
				continue
			fi

			dependency_path=$(resolve_dependency "$dependency" "$source_path" || true)
			if [ -z "$dependency_path" ]; then
				echo "Cannot resolve non-Apple dependency $dependency from $source_path" >&2
				exit 1
			fi
			if is_apple_dependency "$dependency_path"; then
				continue
			fi

			framework_name=$(basename "$dependency_path")
			framework_path="$frameworks_path/$framework_name"
			if ! seen_source "$dependency_path"; then
				if seen_destination "$framework_path"; then
					echo "Dylib name collision for $framework_name" >&2
					exit 1
				fi
				cp -L "$dependency_path" "$framework_path"
				printf '%s|%s\n' "$dependency_path" "$framework_path" >> "$manifest_path"
				printf '%s|%s\n' "$dependency_path" "$framework_path" >> "$todo_path"
			fi

			if [ "$dependency" != "@rpath/$framework_name" ]; then
				install_name_tool -change "$dependency" "@rpath/$framework_name" "$bundled_path"
			fi
		done <<-EOF
$(otool -L "$source_path" | awk 'NR > 1 { print $1 }')
EOF

		remove_external_rpaths "$bundled_path"
		if [ "$bundled_path" = "$main_executable" ]; then
			add_rpath "$bundled_path" '@executable_path/../Frameworks'
		else
			install_name_tool -id "@rpath/$(basename "$bundled_path")" "$bundled_path"
			add_rpath "$bundled_path" '@loader_path'
		fi
	done < "$current_path"
done

rm -rf "$work_path"
echo "Created macOS bundle: $bundle_path"