import subprocess
import json
import os
from pathlib import Path

def run_command(row, timeout=600.0):
    try:
        # Run the command, capturing stdout and stderr
        result = subprocess.run(
            row['cmd'],                  # command line
            shell=True,                  # allow shell features such as redirection and pipes
            text=True,                   # process output in text mode
            stdout=subprocess.PIPE,      # capture standard output
            stderr=subprocess.PIPE,      # capture standard error
            cwd=row.get('cwd', None),    # specify the working directory
            timeout=timeout              # set the timeout
        )

        # Store the output and the return code in the dictionary
        row['stdout'] = result.stdout
        row['stderr'] = result.stderr
        row['returncode'] = result.returncode
        row['elapsed'] = timeout if result.returncode != 0 else None

    except subprocess.TimeoutExpired:
        # Handle command timeout
        row['stdout'] = ''
        row['stderr'] = 'Process timed out'
        row['returncode'] = -1
        row['elapsed'] = timeout
    except Exception as e:
        # Handle other possible exceptions
        row['stdout'] = ''
        row['stderr'] = f'Error: {e}'
        row['returncode'] = -1
        row['elapsed'] = timeout

    return row

def save_row_to_file(row, filename):
    try:
        # Check whether the file exists; if it exists, append data, otherwise create the file
        if os.path.exists(filename):
            print(f"File already exists: {filename}")
            
        else:
            # If the file does not exist, create a new file and write the data
            with open(filename, 'w', encoding='utf-8') as file:
                json.dump([row], file, ensure_ascii=False, indent=4)
    except Exception as e:
        print(f"Error while saving row to file: {e}")
        
# Example usage
if __name__ == '__main__':
    
    threads = 8
    root = Path("C:\\Users\\RGZN090201\\Documents\\GitHub\\Quantum-Sparse-State-Calculator\\test\\CPUTest\\QASMTest")
    
    executor = Path("C:\\Users\\RGZN090201\\Documents\\GitHub\\Quantum-Sparse-State-Calculator\\build\\x64-Release\\bin") / "QasmTest.exe"
    input = root / "qasm_files" / "bv_n19.qasm"
    filename = root / "bv_test.json"
    # on windows system
    row = {
        'cmd': f'set OMP_NUM_THREADS={threads}\n {executor} --input {input}', # set OMP_NUM_THREADS=threads
    }


    result = run_command(row)
    print(result)
    save_row_to_file(result, filename)
    print("Command:", result['cmd'])
    print("Stdout:", result['stdout'])
    print("Stderr:", result['stderr'])
    print("Return code:", result['returncode'])
