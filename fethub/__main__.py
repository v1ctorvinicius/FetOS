# fethub/__main__.py
import multiprocessing
from fethub.core.tickloop import FetHubCore

def main():
    multiprocessing.set_start_method("spawn", force=True)
    FetHubCore().run()

if __name__ == "__main__":
    main()
