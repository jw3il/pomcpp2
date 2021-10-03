from pathlib import Path
from shutil import copyfile


class AutoCopy:
    """
    Automatically creates copies of a file when called.
    """

    def __init__(self, path: str, copy_prefix: str):
        """
        Create an AutoCopy object to automatically copy files to [copy_prefix]_callno_[path_suffix].

        :param path: The path of the file you want to copy
        :param copy_prefix: The prefix of the target path
        """
        ppath = Path(path)
        assert ppath.exists(), f"{path} does not exist!"
        assert ppath.is_file(), f"{path} is not a file!"

        self.path = path
        self.copy_prefix = copy_prefix
        self.path_suffix = ppath.suffix
        self.calls = 0
        self.copies = []

    def __call__(self, *args, **kwargs):
        self.calls += 1
        new_path = f"{self.copy_prefix}_{self.calls}{self.path_suffix}"
        copyfile(self.path, new_path)
        self.copies.append(new_path)
        return new_path

    def delete_copies(self):
        for copy in self.copies:
            copy_path = Path(copy)
            if copy_path.exists():
                copy_path.unlink()

        self.copies = []
