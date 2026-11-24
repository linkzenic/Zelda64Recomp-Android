import os
import sys
import traceback


def generate(yaml_directory, output_directory):
    previous_directory = os.getcwd()
    previous_arguments = sys.argv[:]

    try:
        os.makedirs(output_directory, exist_ok=True)
        os.chdir(output_directory)
        sys.argv = [
            "MMGenerate.py",
            "--player_files_path",
            yaml_directory,
            "--outputpath",
            output_directory,
        ]

        from MMGenerate import main

        main()
        return True
    except Exception:
        traceback.print_exc()
        return False
    finally:
        sys.argv = previous_arguments
        os.chdir(previous_directory)
