import subprocess

subprocess.call('cd ../doxygen && doxygen Doxyfile && cd ../sphinx', shell=True)

project = 'Caterva'
copyright = '2019, Blosc Developers'

extensions = ["sphinx.ext.mathjax", "breathe"]

source_suffix = '.rst'

master_doc = 'index'

language = None

exclude_patterns = ['.build', 'Thumbs.db', '.DS_Store']

pygments_style = None

html_static_path = [".static"]
html_theme = 'sphinx_rtd_theme'
html_logo = "logo-caterva.png"
html_show_sourcelink = False

html_theme_options = {
    "logo_only": True,
}

breathe_projects = { "include": "../doxygen/xml/" }
breathe_default_project = "include"

def setup(app):
    app.add_css_file('custom.css')