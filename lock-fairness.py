log_file = "oslab-log.txt"

log_counts = {}

with open(log_file, 'r') as file:
    for line in file:
        if line.startswith("[LOCK]"):
            line = line.strip()
            if line in log_counts:
                log_counts[line] += 1
            else:
                log_counts[line] = 1

for log, count in log_counts.items():
    print(f"{log}: {count} times")
