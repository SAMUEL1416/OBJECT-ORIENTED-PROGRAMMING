from flask import Flask, render_template, request, redirect, session
import subprocess
import json
import os
import uuid
import sqlite3
from datetime import datetime, timedelta
from werkzeug.security import generate_password_hash, check_password_hash

app = Flask(__name__, template_folder="../templates", static_folder="../static")
app.secret_key = "codeguard_secret_key"

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UPLOAD_DIR = os.path.join(BASE_DIR, "uploads")
DB_PATH = os.path.join(BASE_DIR, "codeguard.db")

# Local Windows
CPP_ENGINE = os.path.join(BASE_DIR, "cpp_engine", "codeguard")

# Render/Linux:
# CPP_ENGINE = os.path.join(BASE_DIR, "cpp_engine", "codeguard")

os.makedirs(UPLOAD_DIR, exist_ok=True)


def indian_time():
    return (datetime.utcnow() + timedelta(hours=5, minutes=30)).strftime("%Y-%m-%d %I:%M:%S %p")


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
        created_at TEXT,
        FOREIGN KEY(user_id) REFERENCES users(id)
    )
    """)

    conn.commit()
    conn.close()


init_db()


def prepare_dashboard_data(result):
    errors = result.get("errors", [])
    real_errors = [e for e in errors if e.get("type") != "No Major Issue Found"]

    high_errors = []
    medium_errors = []
    low_errors = []

    for e in real_errors:
        risk = e.get("risk", "Low")

        if risk == "High":
            high_errors.append(e)
        elif risk == "Medium":
            medium_errors.append(e)
        elif risk == "Low":
            low_errors.append(e)

    high_count = len(high_errors)
    medium_count = len(medium_errors)
    low_count = len(low_errors)
    total_errors = len(real_errors)

    if total_errors == 0:
        score = 100
        crash_probability = 0
        overall_risk = "Safe"
        status_message = "Excellent! No issue was detected."
        recommendation = "Your code appears safe based on current analysis."

    elif high_count > 0:
        score = max(10, 100 - (high_count * 25) - (medium_count * 10) - (low_count * 5))
        crash_probability = min(95, (high_count * 30) + (medium_count * 12) + (low_count * 5))
        overall_risk = "High"
        status_message = "Your code contains high-risk errors that may crash during execution."
        recommendation = "Fix all high-risk issues immediately."

    elif medium_count > 0:
        score = max(35, 100 - (medium_count * 15) - (low_count * 5))
        crash_probability = min(70, (medium_count * 18) + (low_count * 5))
        overall_risk = "Medium"
        status_message = "Your code has medium-risk issues and should be reviewed."
        recommendation = "Fix medium-risk issues before final execution."

    elif low_count > 0:
        score = max(75, 100 - (low_count * 8))
        crash_probability = min(25, low_count * 8)
        overall_risk = "Low"
        status_message = "Your code is mostly safe with minor improvements."
        recommendation = "Review low-risk suggestions to improve code quality."

    else:
        score = 100
        crash_probability = 0
        overall_risk = "Safe"
        status_message = "Excellent! No issue was detected."
        recommendation = "Your code appears safe based on current analysis."

    learning_topics = []

    for e in real_errors:
        error_type = e.get("type", "")

        if "Array" in error_type:
            learning_topics.append("Array indexing and boundary checking in C++")
        elif "Division" in error_type:
            learning_topics.append("Division by zero and safe arithmetic operations")
        elif "Pointer" in error_type:
            learning_topics.append("Pointer initialization and memory safety")
        elif "Memory" in error_type:
            learning_topics.append("Dynamic memory allocation and delete/delete[] usage")
        elif "Destructor" in error_type:
            learning_topics.append("Constructors and destructors in C++")
        elif "Exception" in error_type:
            learning_topics.append("Exception handling using try-catch blocks")
        elif "Polymorphism" in error_type:
            learning_topics.append("Virtual functions and runtime polymorphism")
        elif "OOP" in error_type:
            learning_topics.append("Class design, access specifiers, and encapsulation")
        elif "Infinite" in error_type:
            learning_topics.append("Loop control and termination conditions")

    learning_topics = list(dict.fromkeys(learning_topics))

    result["errors"] = real_errors if real_errors else [{
        "line": "-",
        "type": "No Major Issue Found",
        "risk": "Safe",
        "message": "No runtime, logical, syntax, or OOP issue detected by current analyzer.",
        "solution": "Code appears safe based on current analysis."
    }]

    return {
        "score": score,
        "high_count": high_count,
        "medium_count": medium_count,
        "low_count": low_count,
        "total_errors": total_errors,
        "crash_probability": crash_probability,
        "overall_risk": overall_risk,
        "status_message": status_message,
        "recommendation": recommendation,
        "learning_topics": learning_topics
    }


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

        try:
            conn = sqlite3.connect(DB_PATH)
            cur = conn.cursor()
            cur.execute(
                "INSERT INTO users (email, password) VALUES (?, ?)",
                (email, generate_password_hash(password))
            )
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

    data = prepare_dashboard_data(result)
    result["score"] = data["score"]
    result["total_errors"] = data["total_errors"]

    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("""
    INSERT INTO history (user_id, code, score, total_errors, result, created_at)
    VALUES (?, ?, ?, ?, ?, ?)
    """, (
        session["user_id"],
        code,
        data["score"],
        data["total_errors"],
        json.dumps(result),
        indian_time()
    ))
    conn.commit()
    conn.close()

    return render_template("dashboard.html", code=code, result=result, data=data, email=session["email"])


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
    ORDER BY id DESC
    """, (session["user_id"],))
    records = cur.fetchall()
    conn.close()

    return render_template("history.html", records=records, email=session["email"])


@app.route("/history/<int:history_id>")
def view_history_result(history_id):
    if "user_id" not in session:
        return redirect("/login")

    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("""
    SELECT code, result
    FROM history
    WHERE id = ? AND user_id = ?
    """, (history_id, session["user_id"]))
    record = cur.fetchone()
    conn.close()

    if not record:
        return redirect("/history")

    code = record[0]
    result = json.loads(record[1])
    data = prepare_dashboard_data(result)

    return render_template("dashboard.html", code=code, result=result, data=data, email=session["email"])


if __name__ == "__main__":
    if __name__ == "__main__":
        app.run(host="0.0.0.0", port=int(os.environ.get("PORT", 10000)))