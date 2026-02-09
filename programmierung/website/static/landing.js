// Landing Page JavaScript - Interactive Effects

// ============================================================================
// PARTICLE BACKGROUND
// ============================================================================

function createParticles() {
    const particlesContainer = document.getElementById('particles');
    if (!particlesContainer) return;
    
    const particleCount = 50;
    
    for (let i = 0; i < particleCount; i++) {
        const particle = document.createElement('div');
        particle.className = 'particle';
        
        // Random position
        const x = Math.random() * 100;
        const y = Math.random() * 100;
        
        // Random size
        const size = Math.random() * 3 + 1;
        
        // Random animation duration
        const duration = Math.random() * 20 + 10;
        
        // Random delay
        const delay = Math.random() * 5;
        
        particle.style.cssText = `
            position: absolute;
            left: ${x}%;
            top: ${y}%;
            width: ${size}px;
            height: ${size}px;
            background: radial-gradient(circle, rgba(102, 126, 234, 0.8) 0%, transparent 70%);
            border-radius: 50%;
            animation: float ${duration}s ease-in-out ${delay}s infinite;
            pointer-events: none;
        `;
        
        particlesContainer.appendChild(particle);
    }
    
    // Add CSS animation
    const style = document.createElement('style');
    style.textContent = `
        @keyframes float {
            0%, 100% {
                transform: translate(0, 0) scale(1);
                opacity: 0.3;
            }
            25% {
                transform: translate(10px, -20px) scale(1.1);
                opacity: 0.6;
            }
            50% {
                transform: translate(-10px, -40px) scale(0.9);
                opacity: 0.4;
            }
            75% {
                transform: translate(15px, -20px) scale(1.05);
                opacity: 0.5;
            }
        }
    `;
    document.head.appendChild(style);
}

// ============================================================================
// SMOOTH SCROLLING
// ============================================================================

function initSmoothScroll() {
    document.querySelectorAll('a[href^="#"]').forEach(anchor => {
        anchor.addEventListener('click', function (e) {
            const href = this.getAttribute('href');
            if (href === '#') return;
            
            e.preventDefault();
            const target = document.querySelector(href);
            
            if (target) {
                target.scrollIntoView({
                    behavior: 'smooth',
                    block: 'start'
                });
            }
        });
    });
}

// ============================================================================
// SCROLL ANIMATIONS
// ============================================================================

function initScrollAnimations() {
    const observerOptions = {
        threshold: 0.1,
        rootMargin: '0px 0px -100px 0px'
    };
    
    const observer = new IntersectionObserver((entries) => {
        entries.forEach(entry => {
            if (entry.isIntersecting) {
                entry.target.style.opacity = '1';
                entry.target.style.transform = 'translateY(0)';
            }
        });
    }, observerOptions);
    
    // Observe feature cards
    document.querySelectorAll('.feature-card').forEach((card, index) => {
        card.style.opacity = '0';
        card.style.transform = 'translateY(30px)';
        card.style.transition = `all 0.6s ease ${index * 0.1}s`;
        observer.observe(card);
    });
    
    // Observe architecture section
    const archElements = document.querySelectorAll('.architecture-text, .architecture-visual');
    archElements.forEach((el, index) => {
        el.style.opacity = '0';
        el.style.transform = 'translateY(30px)';
        el.style.transition = `all 0.8s ease ${index * 0.2}s`;
        observer.observe(el);
    });
}

// ============================================================================
// NAVBAR SCROLL EFFECT
// ============================================================================

function initNavbarScroll() {
    const navbar = document.querySelector('.navbar');
    if (!navbar) return;
    
    let lastScroll = 0;
    
    window.addEventListener('scroll', () => {
        const currentScroll = window.pageYOffset;
        
        if (currentScroll > 100) {
            navbar.style.background = 'rgba(10, 14, 39, 0.9)';
            navbar.style.backdropFilter = 'blur(10px)';
            navbar.style.borderBottom = '1px solid rgba(255, 255, 255, 0.1)';
        } else {
            navbar.style.background = 'transparent';
            navbar.style.backdropFilter = 'none';
            navbar.style.borderBottom = 'none';
        }
        
        lastScroll = currentScroll;
    });
}

// ============================================================================
// INTERACTIVE DIAGRAM
// ============================================================================

function initDiagram() {
    const nodes = document.querySelectorAll('.diagram-node');
    
    nodes.forEach(node => {
        node.addEventListener('mouseenter', () => {
            // Add glow effect
            node.style.boxShadow = '0 0 30px rgba(102, 126, 234, 0.6)';
        });
        
        node.addEventListener('mouseleave', () => {
            node.style.boxShadow = 'none';
        });
    });
}

// ============================================================================
// STATS COUNTER ANIMATION
// ============================================================================

function animateStats() {
    const stats = document.querySelectorAll('.stat-value');
    
    const observerOptions = {
        threshold: 0.5
    };
    
    const observer = new IntersectionObserver((entries) => {
        entries.forEach(entry => {
            if (entry.isIntersecting && !entry.target.classList.contains('counted')) {
                entry.target.classList.add('counted');
                
                const text = entry.target.textContent;
                if (text === 'Live') return; // Skip non-numeric
                
                const target = parseInt(text.replace('+', ''));
                const hasPlus = text.includes('+');
                let current = 0;
                const increment = target / 30;
                const duration = 1000;
                const stepTime = duration / 30;
                
                const timer = setInterval(() => {
                    current += increment;
                    if (current >= target) {
                        entry.target.textContent = target + (hasPlus ? '+' : '');
                        clearInterval(timer);
                    } else {
                        entry.target.textContent = Math.floor(current) + (hasPlus ? '+' : '');
                    }
                }, stepTime);
            }
        });
    }, observerOptions);
    
    stats.forEach(stat => observer.observe(stat));
}

// ============================================================================
// MOUSE PARALLAX EFFECT
// ============================================================================

function initParallax() {
    const hero = document.querySelector('.hero');
    if (!hero) return;
    
    hero.addEventListener('mousemove', (e) => {
        const { clientX, clientY } = e;
        const { innerWidth, innerHeight } = window;
        
        const xPos = (clientX / innerWidth - 0.5) * 20;
        const yPos = (clientY / innerHeight - 0.5) * 20;
        
        const particles = document.getElementById('particles');
        if (particles) {
            particles.style.transform = `translate(${xPos}px, ${yPos}px)`;
        }
    });
}

// ============================================================================
// INITIALIZATION
// ============================================================================

document.addEventListener('DOMContentLoaded', () => {
    createParticles();
    initSmoothScroll();
    initScrollAnimations();
    initNavbarScroll();
    initDiagram();
    animateStats();
    initParallax();
    
    console.log('🚀 Energiesystem Lausitz - Landing Page loaded');
});

// ============================================================================
// PERFORMANCE OPTIMIZATION
// ============================================================================

// Reduce animations on low-end devices
if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
    document.querySelectorAll('*').forEach(el => {
        el.style.animation = 'none';
        el.style.transition = 'none';
    });
}
