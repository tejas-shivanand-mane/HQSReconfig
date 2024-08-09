import re
from datetime import datetime
from dataclasses import dataclass
import matplotlib.pyplot as plt
import pandas as pd

@dataclass
class Ledger:
    sequence_number: int
    timestamp: datetime
    close_time: datetime

    def __str__(self):
        return f"Ledger(sequence_number={self.sequence_number}, timestamp={self.timestamp}, close_time={self.close_time})"

def parse_ledger_info(log_file):
    ledger_info_list = []

    # Regular expression to match the log line
    pattern = r"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}) [\w\d]+ \[Ledger INFO\] Closed ledger: \[seq=(\d+), hash=[\w\d]+\] at (\d+)"

    with open(log_file, 'r') as file:
        for line in file:
            match = re.search(pattern, line)
            if match:
                timestamp_str = match.group(1)
                sequence = int(match.group(2))
                close_time_ms = int(match.group(3))

                # Convert timestamp string to datetime object
                timestamp = datetime.strptime(timestamp_str, '%Y-%m-%dT%H:%M:%S.%f')

                # Convert milliseconds since epoch to datetime
                #close_time = datetime.fromtimestamp(close_time_ms / 1000.0)
                close_time = close_time_ms

                # Create a Ledger object and add it to the list
                ledger = Ledger(sequence_number=sequence, timestamp=timestamp, close_time=close_time)
                ledger_info_list.append(ledger)

    return ledger_info_list

def calculate_throughput(ledgers):
    throughputs = []
    for i in range(1, len(ledgers)):
        previous_ledger = ledgers[i - 1]
        current_ledger = ledgers[i]

        # Time difference in milliseconds
        time_diff = (current_ledger.close_time - previous_ledger.close_time).total_seconds() * 1000.0
        
        if time_diff > 0:
            # Throughput: ledgers closed per millisecond
            throughput = 1 / time_diff
            throughputs.append({
                'sequence': current_ledger.sequence_number,
                'throughput': throughput,
                'timestamp': current_ledger.close_time
            })
    
    return pd.DataFrame(throughputs)

def plot_throughputs(throughput_data_list, labels):
    plt.figure(figsize=(10, 6))
    
    for throughput_data, label in zip(throughput_data_list, labels):
        plt.plot(throughput_data['timestamp'], throughput_data['throughput'], marker='o', label=label)
    
    plt.xlabel('Timestamp')
    plt.ylabel('Throughput (Ledgers per Millisecond)')
    plt.title('Ledger Throughput Over Time')
    plt.legend()
    plt.grid(True)
    plt.show()

if __name__ == "__main__":
    print('start')
    ledgers1 = parse_ledger_info('./stellar1_log.txt')
    #ledgers2 = parse_ledger_info('./stellar2_log.txt')
    #ledgers3 = parse_ledger_info('./stellar3_log.txt')
    ledgers4 = parse_ledger_info('./stellar4_log.txt')
    if ledgers1:
        for ledger in ledgers1:
            print(ledger)

    # Calculate throughput data for each ledger array
    throughput_data1 = calculate_throughput(ledgers1)
    #throughput_data2 = calculate_throughput(ledgers2)
    #throughput_data3 = calculate_throughput(ledgers3)
    throughput_data4 = calculate_throughput(ledgers4)

    # Plot throughput for all ledger arrays on one graph
    #plot_throughputs([throughput_data1, throughput_data2, throughput_data3, throughput_data4],
    #                 ['Ledger Set 1', 'Ledger Set 2', 'Ledger Set 3', 'Ledger Set 4'])
    plot_throughputs([throughput_data1, throughput_data4],
                     ['Ledger Set 1', 'Ledger Set 4'])