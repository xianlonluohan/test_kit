import argparse
import base64
import mimetypes
import os
import re
import shutil
import sys
import zipfile
from pathlib import Path

from bs4 import BeautifulSoup
from markdown import markdown
from playwright.sync_api import sync_playwright
from pymdownx import superfences
from pymdownx.emoji import gemoji, to_alt


IGNORE_DIRS = {
    ".git",
    ".github",
    ".vscode",
    "__pycache__",
    ".idea",
    ".DS_Store",
    "node_modules",
}
IMAGE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".gif", ".bmp", ".svg", ".webp", ".ico"}
MD_EXTENSIONS = {".md", ".markdown"}

MARKDONW_CSS = "markdown.css"


def slugify_unicode(text, separator="-"):
    if not text:
        return ""
    text = re.sub(r"[^\w\u4e00-\u9fff]+", separator, text.strip())
    return text.strip(separator).lower()


def should_copy(file_path):
    ext = os.path.splitext(file_path)[1].lower()
    return ext not in MD_EXTENSIONS and ext not in IMAGE_EXTENSIONS


def embed_images_as_base64(parsed_html, base_dir):
    for img in parsed_html.find_all("img"):
        src = img.get("src")
        if not src or src.startswith(("http://", "https://", "file://", "//", "data:")):
            continue

        abs_path = base_dir / src
        if not abs_path.exists():
            print(f"警告：图片文件不存在 {abs_path}")
            continue

        try:
            with open(abs_path, "rb") as img_file:
                img_data = img_file.read()
            mime_type, _ = mimetypes.guess_type(abs_path)
            if not mime_type:
                if abs_path.suffix.lower() in [".jpg", ".jpeg"]:
                    mime_type = "image/jpeg"
                else:
                    mime_type = "image/png"
            b64_data = base64.b64encode(img_data).decode("utf-8")
            img["src"] = f"data:{mime_type};base64,{b64_data}"
        except Exception as e:
            print(f"警告：无法读取图片 {abs_path}: {e}")


def convert_markdown_to_html(md_text, base_dir):
    extensions = [
        "toc",
        "extra",
        "mdx_math",
        "markdown_checklist.extension",
        "pymdownx.magiclink",
        "pymdownx.caret",
        "pymdownx.superfences",
        "pymdownx.betterem",
        "pymdownx.mark",
        "pymdownx.highlight",
        "pymdownx.tasklist",
        "pymdownx.tilde",
        "pymdownx.emoji",
        "attr_list",
    ]
    extension_configs = {
        "mdx_math": {"enable_dollar_delimiter": True},
        "pymdownx.superfences": {
            "custom_fences": [
                {
                    "name": "mermaid",
                    "class": "mermaid",
                    "format": superfences.fence_div_format,
                }
            ]
        },
        "pymdownx.highlight": {"linenums": False},
        "pymdownx.tasklist": {"clickable_checkbox": True},
        "pymdownx.emoji": {"emoji_index": gemoji, "emoji_generator": to_alt},
        "toc": {"slugify": slugify_unicode, "permalink": ""},
    }

    html_body = markdown(
        md_text,
        output_format="html",
        extensions=extensions,
        extension_configs=extension_configs,
    )

    parsed_html = BeautifulSoup(html_body, "html.parser")

    for header in parsed_html.find_all(["h1", "h2", "h3", "h4", "h5", "h6"]):
        if not header.get("id"):
            header["id"] = slugify_unicode(header.get_text())

    for a in parsed_html.find_all("a", href=True):
        href = a["href"]
        if href.startswith("#") and len(href) > 1:
            target = href[1:]
            if not parsed_html.find(id=target):
                slug_target = slugify_unicode(target)
                if parsed_html.find(id=slug_target):
                    a["href"] = f"#{slug_target}"

    embed_images_as_base64(parsed_html, base_dir)

    for img in parsed_html.find_all("img"):
        style_parts = []
        if img.get("width"):
            style_parts.append(f"width: {img['width']}px;")
            del img["width"]
        if img.get("height"):
            style_parts.append(f"height: {img['height']}px;")
            del img["height"]
        if style_parts:
            existing = img.get("style", "")
            img["style"] = existing + " " + " ".join(style_parts)

    return str(parsed_html)


def convert_to_pdf(
    page, input_path, output_path, watermark, watermark_text, css_content
):
    try:
        with open(input_path, encoding="utf-8") as f:
            md_text = f.read()

        base_dir = Path(input_path).parent.resolve()
        title = Path(input_path).stem

        html_body = convert_markdown_to_html(md_text, base_dir)

        full_html = f"""<!DOCTYPE html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, minimal-ui">
    <title>{title}</title>
    <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/katex/dist/katex.min.css">
    <script src="https://unpkg.com/mermaid@8.7.0/dist/mermaid.min.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/katex/dist/katex.min.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/katex/dist/contrib/mathtex-script-type.min.js" defer></script>
    <style>{css_content}</style>
</head>
<body>
    <article class="markdown-body">{html_body}</article>
</body>
</html>"""

        page.set_content(full_html)
        page.wait_for_selector(".markdown-body", timeout=10000)
        page.wait_for_function(
            """() => {
            const imgs = Array.from(document.querySelectorAll('img'));
            return Promise.all(imgs.map(img => {
                if (img.complete) return true;
                return new Promise(resolve => { img.onload = resolve; img.onerror = resolve; });
            }));
        }""",
            timeout=15000,
        )
        page.wait_for_timeout(1000)

        if watermark:
            page.evaluate(
                f"""() => {{
                    const wm = document.createElement('div');
                    wm.textContent = '{watermark_text}';
                    wm.className = 'watermark';
                    document.body.appendChild(wm);
                }}"""
            )

        page.pdf(path=output_path, print_background=True, prefer_css_page_size=True)
        print(f"✓ 转换成功: {output_path}")
    except Exception as e:
        print(f"✗ 转换失败: {input_path} -> {e}")


def single_markdown_to_pdf(input_path, output_path, watermark, watermark_text):
    css_path = os.path.join(os.path.dirname(__file__), MARKDONW_CSS)
    with open(css_path, encoding="utf-8") as f:
        css_content = f.read()

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()
        convert_to_pdf(
            page, input_path, output_path, watermark, watermark_text, css_content
        )
        browser.close()


def batch_markdown_to_pdf(
    input_dir,
    output_dir,
    watermark,
    watermark_text,
    zip_name,
):
    css_path = os.path.join(os.path.dirname(__file__), MARKDONW_CSS)
    with open(css_path, encoding="utf-8") as f:
        css_content = f.read()

    input_abs = os.path.abspath(input_dir)
    output_abs = os.path.abspath(output_dir)

    work_dir = os.path.join(output_abs, ".tmp_convert")
    if os.path.exists(work_dir):
        shutil.rmtree(work_dir)
    os.makedirs(work_dir, exist_ok=True)

    total_md = 0
    total_other = 0
    converted = 0
    copied = 0

    playwright_ctx = sync_playwright().start()
    browser = playwright_ctx.chromium.launch(headless=True)

    try:
        for root, dirs, files in os.walk(input_abs):
            root_abs = os.path.abspath(root)

            if root_abs.startswith(work_dir):
                dirs[:] = []
                continue

            if root_abs == output_abs:
                tmp_dir_name = os.path.basename(work_dir)
                if tmp_dir_name in dirs:
                    dirs.remove(tmp_dir_name)

            dirs[:] = [d for d in dirs if d not in IGNORE_DIRS]

            md_files_in_dir = [
                f for f in files if f.lower().endswith((".md", ".markdown"))
            ]
            other_files_in_dir = [
                f for f in files if should_copy(os.path.join(root, f))
            ]

            total_md += len(md_files_in_dir)
            total_other += len(other_files_in_dir)

            for md_file in md_files_in_dir:
                src_path = os.path.join(root, md_file)
                if os.path.abspath(src_path).startswith(work_dir):
                    continue

                rel_path = os.path.relpath(src_path, input_abs)
                output_pdf = os.path.join(
                    work_dir, os.path.splitext(rel_path)[0] + ".pdf"
                )
                os.makedirs(os.path.dirname(output_pdf), exist_ok=True)

                if os.path.exists(output_pdf):
                    print(f"强制覆盖已存在 PDF: {output_pdf}")
                else:
                    print(f"生成新 PDF: {output_pdf}")

                page = browser.new_page()
                try:
                    convert_to_pdf(
                        page,
                        src_path,
                        output_pdf,
                        watermark,
                        watermark_text,
                        css_content,
                    )
                    converted += 1
                except Exception as e:
                    print(f"转换失败 {src_path}: {e}")
                finally:
                    page.close()

            for other_file in other_files_in_dir:
                src_path = os.path.join(root, other_file)
                if os.path.abspath(src_path).startswith(work_dir):
                    continue

                rel_path = os.path.relpath(src_path, input_abs)
                dst_path = os.path.join(work_dir, rel_path)
                os.makedirs(os.path.dirname(dst_path), exist_ok=True)

                if os.path.exists(dst_path):
                    print(f"强制覆盖已存在文件: {rel_path}")
                else:
                    print(f"复制新文件: {rel_path}")

                try:
                    shutil.copy2(src_path, dst_path)
                    copied += 1
                except Exception as e:
                    print(f"复制失败 {src_path}: {e}")

        print(f"\n转换与复制完成。")
        print(f"Markdown 文件总数: {total_md}, 成功转换: {converted}")
        print(f"其他文件总数: {total_other}, 成功复制: {copied}")

        zip_filename = zip_name if zip_name else "pdf-documents"
        zip_path = os.path.join(output_abs, f"{zip_filename}.zip")
        if os.path.exists(zip_path):
            print(f"强制覆盖已存在的 ZIP: {zip_path}")
            os.remove(zip_path)

        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
            for zroot, _, zfiles in os.walk(work_dir):
                for file in zfiles:
                    full_path = os.path.join(zroot, file)
                    arcname = os.path.relpath(full_path, work_dir)
                    zf.write(full_path, arcname)

        shutil.rmtree(work_dir)
        print(f"打包完成: {zip_path}")

    finally:
        browser.close()
        playwright_ctx.stop()

def main():
    parser = argparse.ArgumentParser(description="Markdown 转 PDF 工具")
    parser.add_argument(
        "--input", required=True, help="输入文件（.md/.markdown）或目录"
    )
    parser.add_argument("--output", default=None, help="输出文件或目录（可选）")

    watermark_group = parser.add_mutually_exclusive_group()
    watermark_group.add_argument(
        "--watermark", nargs=1, metavar="TEXT", help="启用水印并设置水印文本"
    )
    watermark_group.add_argument(
        "--no-watermark", action="store_true", help="禁用所有水印"
    )

    parser.add_argument(
        "--zip-name",
        type=str,
        default="pdf-documents",
        help="ZIP 压缩包名称（不含扩展名），默认为 pdf-documents",
    )

    args = parser.parse_args()

    if args.no_watermark:
        use_watermark = False
        watermark_text = ""
    elif args.watermark is not None:
        use_watermark = True
        watermark_text = args.watermark[0]
    else:
        use_watermark = True
        watermark_text = "emakefun"

    input_path = args.input
    output_path = args.output

    if not os.path.exists(input_path):
        print(f"错误：输入路径不存在 - {input_path}")
        sys.exit(1)

    if os.path.isfile(input_path):
        if not input_path.lower().endswith((".md", ".markdown")):
            print(
                f"错误：输入文件必须是 Markdown 文件（扩展名 .md 或 .markdown），"
                f"当前文件：{input_path}"
            )
            sys.exit(1)

        if output_path is None:
            output_pdf = os.path.splitext(input_path)[0] + ".pdf"
        else:
            if output_path.lower().endswith(".pdf"):
                output_pdf = output_path
                out_dir = os.path.dirname(output_pdf)
                if out_dir and not os.path.exists(out_dir):
                    os.makedirs(out_dir, exist_ok=True)
            else:
                if not os.path.exists(output_path):
                    os.makedirs(output_path, exist_ok=True)
                base_name = os.path.splitext(os.path.basename(input_path))[0]
                output_pdf = os.path.join(output_path, base_name + ".pdf")

        if not output_pdf.lower().endswith(".pdf"):
            print(f"错误：单文件模式下输出路径必须以 .pdf 结尾，当前：{output_pdf}")
            sys.exit(1)

        single_markdown_to_pdf(
            input_path,
            output_pdf,
            watermark=use_watermark,
            watermark_text=watermark_text,
        )

    elif os.path.isdir(input_path):
        if output_path is None:
            output_dir = input_path
        else:
            if os.path.exists(output_path):
                if not os.path.isdir(output_path):
                    print(
                        f"错误：批量模式下输出必须是一个目录，"
                        f"但当前路径是一个文件：{output_path}"
                    )
                    sys.exit(1)
            else:
                basename = os.path.basename(output_path)
                if "." in basename and basename not in (".", ".."):
                    print(
                        f"错误：批量模式下输出必须是一个目录，"
                        f"不能是文件路径（如 '{output_path}'）。"
                        "请指定一个目录路径。"
                    )
                    sys.exit(1)
            output_dir = output_path

        os.makedirs(output_dir, exist_ok=True)

        batch_markdown_to_pdf(
            input_dir=input_path,
            output_dir=output_dir,
            watermark=use_watermark,
            watermark_text=watermark_text,
            zip_name=args.zip_name,
        )
    else:
        print(f"错误：输入路径既不是文件也不是目录：{input_path}")
        sys.exit(1)


if __name__ == "__main__":
    main()
    
