import os
import matplotlib.pyplot as plt
import re
from datetime import datetime
import seaborn as sns
import pandas as pd

# Paths
log_dir = "/home/maoziming/rocksdb/backend/build/logs"
output_dir = "figures/"

# Create output dir if it doesn't exist
os.makedirs(output_dir, exist_ok=True)

# Benchmarks and Datasets
# BENCHMARKS = ["invalidate_bench", "ttl_bench", "stale_bench", "update_bench"]

BENCHMARKS = ["stale_bench",  "ttl_bench", "invalidate_bench", "update_bench"]
DATASETS = ["Meta", "Twitter", "Tencent", "IBM", "Alibaba"]

    
dataset_to_reqs = {
    "Meta": 500000,
    "Twitter": 5000000,
    "IBM": 30000,
    "Tencent": 100000, 
    "Alibaba": 300000,
    "PoissonMix": 200000
}

benchmark_to_print_name = {
    "stale_bench": "TTL (Inf.)",
    "ttl_bench": "TTL (1s)",
    "invalidate_bench": "Inv.", 
    "update_bench": "Upd."
}


# Regex to match the log files
log_filename_pattern = re.compile(r"(\w+)_([^_]+)_(\d{8}_\d{6})\.log")

# Function to find the latest log for a given benchmark and dataset
def find_latest_log(benchmark, dataset):
    latest_time = None
    latest_log = None

    for log_file in os.listdir(log_dir):
        match = log_filename_pattern.match(log_file)
        if match:
            log_benchmark, log_dataset, timestamp_str = match.groups()
            if log_benchmark == benchmark and log_dataset == dataset:
                timestamp = datetime.strptime(timestamp_str, "%Y%m%d_%H%M%S")
                if latest_time is None or timestamp > latest_time:
                    latest_time = timestamp
                    latest_log = log_file
    return latest_log

# Lists to store extracted data
timestamps = []
cpu_utilizations = []
network_recv_mb = []
network_send_mb = []

# Regular expression to parse the log line
log_pattern = re.compile(
    r"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) - CPU Utilization: ([\d.]+)% \| "
    r"usr: [\d.]+%, sys: [\d.]+%, idle: [\d.]+%, iowait: [\d.]+%, steal: [\d.]+% \| "
    r"Network recv: (\d+) bytes, send: (\d+) bytes"
)

# Plotting parameters
BIG_SIZE = 10
FIGRATIO = 1 / 3
FIGWIDTH = 7
FIGHEIGHT = FIGWIDTH * FIGRATIO
FIGSIZE = (FIGWIDTH, FIGHEIGHT)

plt.rcParams.update({
    "figure.figsize": FIGSIZE,
    "figure.dpi": 300,
})

COLORS = sns.color_palette("Paired")
sns.set_style("ticks")
sns.set_palette(COLORS)

plt.rc("font", size=BIG_SIZE)
plt.rc("axes", titlesize=BIG_SIZE)
plt.rc("axes", labelsize=BIG_SIZE)
plt.rc("xtick", labelsize=BIG_SIZE)
plt.rc("ytick", labelsize=BIG_SIZE)
plt.rc("legend", fontsize=BIG_SIZE)
plt.rc("figure", titlesize=BIG_SIZE)

max_times_data = {"Benchmark": [], "Dataset": [], "Throughput": []}
dataset_to_max_throughput = {
    
}

def get_throughput(dataset, time): 
    
    return dataset_to_reqs[dataset] / time

# Process each benchmark and dataset combination
for dataset in DATASETS:
    for benchmark in BENCHMARKS:
        log_file = find_latest_log(benchmark, dataset)
        if log_file:
            log_path = os.path.join(log_dir, log_file)
            # print(f"Processing log file: {log_path}")

            # Read and process the log file
            timestamps.clear()
            cpu_utilizations.clear()
            network_recv_mb.clear()
            network_send_mb.clear()

            with open(log_path, "r") as file:
                for line in file:
                    match = log_pattern.match(line)
                    if match:
                        timestamp_str = match.group(1)
                        cpu_utilization = float(match.group(2))
                        recv_bytes = int(match.group(3)) / (1024 * 1024)  # Convert to MB
                        send_bytes = int(match.group(4)) / (1024 * 1024)  # Convert to MB

                        timestamp = datetime.strptime(timestamp_str, "%Y-%m-%d %H:%M:%S")

                        timestamps.append(timestamp)
                        cpu_utilizations.append(cpu_utilization)
                        network_recv_mb.append(recv_bytes)
                        network_send_mb.append(send_bytes)

            if not timestamps:
                print(f"No data found in {log_file}")
                continue

            start_time = timestamps[0]
            relative_times = [(ts - start_time).total_seconds() for ts in timestamps]

            # Create the figure and axis objects
            fig, ax1 = plt.subplots()

            # Plot network statistics on the primary y-axis
            ax1.plot(relative_times, network_recv_mb, label="Recv (MB/s)", color="green")
            ax1.plot(relative_times, network_send_mb, label="Send (MB/s)", color="red")

            ax1.set_xlabel("Time")
            ax1.set_ylabel("Network Traffic (MB/s)", color="black")
            ax1.tick_params(axis='y')

            ax1.xaxis.set_major_locator(plt.MaxNLocator(integer=True))
            ax1.xaxis.set_major_formatter(plt.FuncFormatter(lambda x, _: f"{int(x)} s"))

            # Add the max relative time to the data dictionary
            max_times_data["Benchmark"].append(benchmark)
            max_times_data["Dataset"].append(dataset)
            max_times_data["Throughput"].append(get_throughput(dataset, max(relative_times)))
            dataset_to_max_throughput[dataset] = max(dataset_to_max_throughput.get(dataset, 0), get_throughput(dataset, max(relative_times)))
            # ax1.set_xlim([0, 300])

            # Create a secondary y-axis for CPU utilization
            ax2 = ax1.twinx()
            ax2.plot(relative_times, cpu_utilizations, label="CPU Utilization (%)", color="blue", linestyle="--")
            ax2.set_ylabel("CPU Utilization (%)", color="blue")
            ax2.tick_params(axis='y', labelcolor="blue")

            ax1.legend(loc="upper left")
            ax2.legend(loc="upper right")
            ax1.grid(True)

            # Save the plot to the figures directory
            output_file = os.path.join(output_dir, f"{dataset}_{benchmark}.pdf")
            plt.tight_layout()
            plt.savefig(output_file)
            plt.close()

            # print(f"Figure saved: {output_file}")
            print(benchmark, dataset, max(relative_times))
        else:
            print(f"No log file found for {benchmark} and {dataset}")

# DataFrame to hold raw throughput values
throughput_df = pd.DataFrame(max_times_data)
normalized_throughput_data = {"Benchmark": [], "Dataset": [], "NormalizedThroughput": []}

for index, row in throughput_df.iterrows():
    dataset = row["Dataset"]
    benchmark = row["Benchmark"]
    throughput = row["Throughput"]

    
    # Normalize using stale_bench throughput for the same dataset
    if dataset in dataset_to_max_throughput:
        normalized_value = throughput / dataset_to_max_throughput[dataset] * 100
    else:
        normalized_value = None  # If there's no stale_bench for that dataset, leave it as None

    
    if dataset == "PoissonMix":
        dataset = "Poisson"
    
    # Store normalized throughput
    benchmark = benchmark_to_print_name[benchmark]
    normalized_throughput_data["Benchmark"].append(benchmark)
    normalized_throughput_data["Dataset"].append(dataset)
    normalized_throughput_data["NormalizedThroughput"].append(normalized_value)

# Create a DataFrame from the normalized throughput data
normalized_df = pd.DataFrame(normalized_throughput_data)

BIG_SIZE = 10
FIGRATIO = 2 / 5
FIGWIDTH = 6
FIGHEIGHT = FIGWIDTH * FIGRATIO
FIGSIZE = (FIGWIDTH, FIGHEIGHT)

plt.rcParams.update({
    "figure.figsize": FIGSIZE,
    "figure.dpi": 300,
})

plt.rc("font", size=BIG_SIZE)
plt.rc("axes", titlesize=BIG_SIZE)
plt.rc("axes", labelsize=BIG_SIZE)
plt.rc("xtick", labelsize=BIG_SIZE)
plt.rc("ytick", labelsize=BIG_SIZE)
plt.rc("legend", fontsize=BIG_SIZE)
plt.rc("figure", titlesize=BIG_SIZE)

# Create a DataFrame from the max_times_data dictionary
# max_times_df = pd.DataFrame(max_times_data)

# Plot the grouped bar chart
sns.barplot(x="Dataset", y="NormalizedThroughput", hue="Benchmark", data=normalized_df)
plt.ylim(30)
# Set plot labels and title
plt.xlabel("Workloads")
plt.ylabel("Norm. Thpt (%)")

# Save the grouped bar plot
bar_output_file = os.path.join(output_dir, "throughput_comparison.pdf")
plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.3), ncol=4)

plt.tight_layout()
plt.savefig(bar_output_file)
plt.close()

print(f"Grouped bar plot saved: {bar_output_file}")