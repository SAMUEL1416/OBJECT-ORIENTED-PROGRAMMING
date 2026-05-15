from flask import Flask, render_template, request, redirect, session
import subprocess
import json
import os
import uuid
import sqlite3
from werkzeug.security import generate_password_hash, check_password_hash

app = Flask(
    __name__,
    template_folder="../templates",
    static_folder="../static"
)

app.secret_key = "codeguard_secret_key"

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UPLOAD_DIR = os.path.join(BASE_DIR, "uploads")
DB_PATH = os.path.join(BASE_DIR, "codeguard.db")

CPP_ENGINE = os.path.join(BASE_DIR, "cpp_engine", "codeguard")

os.makedirs(UPLOAD_DIR, exist_ok=True)


def init_db():
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()

    cur.execute("""
    CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        email TEXT UNIQUE NOT NULL,
        password TEXT NOT NULL
    )
    """)

    cur.execute("""
    CREATE TABLE IF NOT EXISTS history (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER NOT NULL,
        code TEXT NOT NULL,
        score INTEGER,
        total_errors INTEGER,
        result TEXT,
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY(user_id) REFERENCES users(id)
    )
    """)

    conn.commit()
    conn.close()


init_db()


@app.route("/")
def home():
    if "user_id" in session:
        return redirect("/index")
    return redirect("/login")


@app.route("/signup", methods=["GET", "POST"])
def signup():
    if request.method == "POST":
        email = request.form.get("email").strip()
        password = request.form.get("password")
        confirm_password = request.form.get("confirm_password")

        if not email.endswith("@gmail.com"):
            return render_template("signup.html", error="Only @gmail.com email is allowed.")

        if password != confirm_password:
            return render_template("signup.html", error="Passwords do not match.")

        hashed_password = generate_password_hash(password)

        try:
            conn = sqlite3.connect(DB_PATH)
            cur = conn.cursor()
            cur.execute("INSERT INTO users (email, password) VALUES (?, ?)", (email, hashed_password))
            conn.commit()
            conn.close()
            return redirect("/login")
        except sqlite3.IntegrityError:
            return render_template("signup.html", error="Account already exists. Please login.")

    return render_template("signup.html")


@app.route("/login", methods=["GET", "POST"])
def login():
    if request.method == "POST":
        email = request.form.get("email").strip()
        password = request.form.get("password")

        conn = sqlite3.connect(DB_PATH)
        cur = conn.cursor()
        cur.execute("SELECT id, password FROM users WHERE email = ?", (email,))
        user = cur.fetchone()
        conn.close()

        if user and check_password_hash(user[1], password):
            session["user_id"] = user[0]
            session["email"] = email
            return redirect("/index")
        else:
            return render_template("login.html", error="Invalid email or password.")

    return render_template("login.html")


@app.route("/logout")
def logout():
    session.clear()
    return redirect("/login")


@app.route("/index")
def index():
    if "user_id" not in session:
        return redirect("/login")

    return render_template("index.html", email=session["email"])


@app.route("/analyze", methods=["POST"])
def analyze():
    if "user_id" not in session:
        return redirect("/login")

    code = request.form.get("code")

    if not code or not code.strip():
        return render_template("index.html", error="Please enter C++ code.", email=session["email"])

    file_id = str(uuid.uuid4())
    input_file = os.path.join(UPLOAD_DIR, file_id + ".cpp")
    output_file = os.path.join(UPLOAD_DIR, file_id + ".json")

    with open(input_file, "w", encoding="utf-8") as f:
        f.write(code)

    subprocess.run([CPP_ENGINE, input_file, output_file])

    with open(output_file, "r", encoding="utf-8") as f:
        result = json.load(f)

    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("""
    INSERT INTO history (user_id, code, score, total_errors, result)
    VALUES (?, ?, ?, ?, ?)
    """, (
        session["user_id"],
        code,
        result["score"],
        result["total_errors"],
        json.dumps(result)
    ))
    conn.commit()
    conn.close()

    return render_template("dashboard.html", code=code, result=result, email=session["email"])


@app.route("/history")
def history():
    if "user_id" not in session:
        return redirect("/login")

    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("""
    SELECT id, code, score, total_errors, created_at
    FROM history
    WHERE user_id = ?
    ORDER BY created_at DESC
    """, (session["user_id"],))
    records = cur.fetchall()
    conn.close()

    return render_template("history.html", records=records, email=session["email"])


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=int(os.environ.get("PORT", 10000)))