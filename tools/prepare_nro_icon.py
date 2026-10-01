"""Convert local square artwork to the embedded 256x256 baseline JPEG icon."""
import argparse
from pathlib import Path
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    with Image.open(args.source) as source:
        if source.width != source.height:
            raise ValueError('NRO icon source must be square')
        image = source.convert('RGB').resize((256, 256), Image.Resampling.LANCZOS)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        image.save(args.output, format='JPEG', quality=95, subsampling=0,
                   progressive=False, exif=b'', icc_profile=None)


if __name__ == '__main__':
    main()
