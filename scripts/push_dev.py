

import sys
import subprocess

num_of_args = len(sys.argv)

if num_of_args > 2: 
    print("too many arguments")
    sys.exit(1)

if num_of_args < 2:
    print("no commit message, add message and resubmit")
    sys.exit(1)

current_branch = subprocess.run(["git", "branch", "--show-current"], capture_output=True, text=True)

branch_name = current_branch.stdout.strip()

commit_message = sys.argv[1].strip()

try:
    subprocess.run(["git", "add", "."], check=True)
except subprocess.SubprocessError:
    print("failed to stage")
    sys.exit(1)

try:
    subprocess.run(["git", "commit", "-m", commit_message], check=True)
except subprocess.SubprocessError:
    print("failed to commit")
    sys.exit(1)

try:
    subprocess.run(["git", "push", "origin", branch_name], check=True)
except subprocess.SubprocessError:
    print("failed to push up")
    sys.exit(1)