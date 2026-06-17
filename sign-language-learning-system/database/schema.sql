CREATE TABLE IF NOT EXISTS users (
    id INT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(50) NOT NULL UNIQUE,
    password_hash VARCHAR(128) NOT NULL,
    email VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS gesture_library (
    id INT PRIMARY KEY AUTO_INCREMENT,
    name VARCHAR(100) NOT NULL,
    category VARCHAR(50) NOT NULL,
    difficulty INT DEFAULT 1,
    description TEXT,
    video_path VARCHAR(500),
    reference_keypoints JSON,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_category (category)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS practice_records (
    id INT PRIMARY KEY AUTO_INCREMENT,
    user_id INT NOT NULL,
    gesture_id INT NOT NULL,
    total_score DECIMAL(5,2) NOT NULL,
    position_score DECIMAL(5,2) NOT NULL,
    angle_score DECIMAL(5,2) NOT NULL,
    stability_score DECIMAL(5,2) NOT NULL,
    feedback_text TEXT NOT NULL,
    practiced_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id),
    FOREIGN KEY (gesture_id) REFERENCES gesture_library(id),
    INDEX idx_user_practice (user_id, practiced_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS wrong_answer_book (
    id INT PRIMARY KEY AUTO_INCREMENT,
    user_id INT NOT NULL,
    gesture_id INT NOT NULL,
    last_score DECIMAL(5,2) NOT NULL,
    error_count INT DEFAULT 1,
    last_wrong_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_user_gesture (user_id, gesture_id),
    FOREIGN KEY (user_id) REFERENCES users(id),
    FOREIGN KEY (gesture_id) REFERENCES gesture_library(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO gesture_library (name, category, difficulty, description, reference_keypoints)
SELECT '你好', '常用词', 1, '常用问候手语动作。', NULL
WHERE NOT EXISTS (SELECT 1 FROM gesture_library WHERE name = '你好');

INSERT INTO gesture_library (name, category, difficulty, description, reference_keypoints)
SELECT '谢谢', '常用词', 1, '表达感谢的手语动作。', NULL
WHERE NOT EXISTS (SELECT 1 FROM gesture_library WHERE name = '谢谢');

INSERT INTO gesture_library (name, category, difficulty, description, reference_keypoints)
SELECT '我', '常用词', 1, '表示自己的手语动作。', NULL
WHERE NOT EXISTS (SELECT 1 FROM gesture_library WHERE name = '我');

INSERT INTO gesture_library (name, category, difficulty, description, reference_keypoints)
SELECT '爱', '常用词', 2, '表示喜爱或爱的手语动作。', NULL
WHERE NOT EXISTS (SELECT 1 FROM gesture_library WHERE name = '爱');

INSERT INTO gesture_library (name, category, difficulty, description, reference_keypoints)
SELECT '学习', '学习生活', 2, '表示学习行为的手语动作。', NULL
WHERE NOT EXISTS (SELECT 1 FROM gesture_library WHERE name = '学习');
