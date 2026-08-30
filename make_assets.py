import os
from PIL import Image

src_img_path = r"C:\Users\nawatpim\.gemini\antigravity-ide\brain\c69641a0-8ce9-494b-89a0-e10c4c6d3879\app_logo_icon_1788079858316.jpg"
res_dir = r"d:\Code\clicker\res"
os.makedirs(res_dir, exist_ok=True)

img = Image.open(src_img_path)

# Save PNG
png_path = os.path.join(res_dir, "app_logo.png")
img.save(png_path, format="PNG")

# Save multi-size ICO
ico_path = os.path.join(res_dir, "app_icon.ico")
icon_sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)]
img.save(ico_path, format="ICO", sizes=icon_sizes)

# Save 56x56 BMP for UI Header
bmp_path = os.path.join(res_dir, "app_logo.bmp")
resized_bmp = img.resize((52, 52), Image.Resampling.LANCZOS)
resized_bmp.save(bmp_path, format="BMP")

print("Created icon and image assets in res/ directory successfully.")
