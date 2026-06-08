import sys
import zipfile


def main():
    args = sys.argv[1:]
    if len(args) < 2:
        print("usage: unzip -Z1 <zip> | unzip -p <zip> <entry>", file=sys.stderr)
        return 2

    mode = args[0]
    zip_path = args[1]
    with zipfile.ZipFile(zip_path) as archive:
        if mode == "-Z1":
            sys.stdout.write("\n".join(archive.namelist()) + "\n")
            return 0
        if mode == "-p" and len(args) >= 3:
            sys.stdout.buffer.write(archive.read(args[2]))
            return 0

    print(f"unsupported unzip arguments: {' '.join(args)}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
