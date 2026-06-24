"""
BTL Validator 아이콘(btl_icon.ico) 생성 스크립트
Windows에서 PyInstaller 빌드 전에 한 번만 실행하면 됩니다.

의존성 (Windows):
    pip install pillow

실행:
    python create_icon.py
"""

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    raise SystemExit("Pillow가 없습니다.  pip install pillow")

import struct, zlib, os

# ── 아이콘 크기별 이미지 생성 ────────────────────────────────────────────
SIZES = [16, 32, 48, 64, 128, 256]


def make_icon_image(size: int) -> Image.Image:
    """BTL 로고 이미지 생성 (파란 배경 + 흰색 'BTL' 텍스트)"""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # 둥근 모서리 파란 사각형 배경
    margin = max(1, size // 16)
    bg_color = (30, 80, 160, 255)       # 짙은 파란색
    draw.rounded_rectangle(
        [margin, margin, size - margin - 1, size - margin - 1],
        radius=max(2, size // 8),
        fill=bg_color,
    )

    # 'BTL' 텍스트
    text = 'BTL'
    font_size = max(6, int(size * 0.38))
    font = None
    for font_path in [
        r'C:\Windows\Fonts\arialbd.ttf',
        r'C:\Windows\Fonts\Arial.ttf',
        r'C:\Windows\Fonts\calibrib.ttf',
    ]:
        try:
            font = ImageFont.truetype(font_path, font_size)
            break
        except Exception:
            continue
    if font is None:
        font = ImageFont.load_default()

    # 텍스트 중앙 정렬
    bbox = draw.textbbox((0, 0), text, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    tx = (size - tw) // 2 - bbox[0]
    ty = (size - th) // 2 - bbox[1]
    draw.text((tx, ty), text, fill=(255, 255, 255, 255), font=font)

    # 하단 작은 'Validator' 텍스트 (32px 이상)
    if size >= 32:
        sub = 'Validator'
        sub_size = max(4, int(size * 0.14))
        sub_font = None
        for fp in [r'C:\Windows\Fonts\arial.ttf', r'C:\Windows\Fonts\calibri.ttf']:
            try:
                sub_font = ImageFont.truetype(fp, sub_size)
                break
            except Exception:
                continue
        if sub_font is None:
            sub_font = ImageFont.load_default()
        sb = draw.textbbox((0, 0), sub, font=sub_font)
        sw = sb[2] - sb[0]
        sx = (size - sw) // 2 - sb[0]
        sy = ty + th + max(1, int(size * 0.04))
        if sy + (sb[3] - sb[1]) < size - margin:
            draw.text((sx, sy), sub, fill=(200, 230, 255, 220), font=sub_font)

    return img


def save_ico(images: list, path: str):
    """여러 크기의 PIL 이미지를 .ico 파일로 저장"""
    # PIL save()로 직접 저장 (간단)
    images[0].save(
        path,
        format='ICO',
        sizes=[(img.width, img.height) for img in images],
        append_images=images[1:],
    )


if __name__ == '__main__':
    imgs = [make_icon_image(s) for s in SIZES]
    out_path = os.path.join(os.path.dirname(__file__), 'btl_icon.ico')
    save_ico(imgs, out_path)
    print(f'아이콘 생성 완료: {out_path}')
    print(f'  포함 크기: {SIZES}')
